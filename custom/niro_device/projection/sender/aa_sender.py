#!/usr/bin/env python3
"""Experimental loopback Android Auto phone-side peer, not a vehicle-ready sender."""

import argparse, json, os, pathlib, select, socket, ssl, struct, subprocess, sys, time

ROOT = pathlib.Path(__file__).resolve().parents[1]


def varint(n):
    if n < 0:
        raise ValueError("Negative unsigned varint")
    out = bytearray()
    while n > 127:
        out.append((n & 127) | 128)
        n >>= 7
    out.append(n)
    return bytes(out)


def pb(field, value):
    if isinstance(value, str):
        value = value.encode()
    if isinstance(value, bytes):
        return varint(field * 8 + 2) + varint(len(value)) + value
    return varint(field * 8) + varint(value)


def fields(data):
    i = 0
    result = {}

    def vi():
        nonlocal i
        n = shift = 0
        while True:
            if i >= len(data) or shift > 63:
                raise ValueError("Malformed protobuf")
            b = data[i]
            i += 1
            n |= (b & 127) << shift
            if b < 128:
                return n
            shift += 7

    while i < len(data):
        tag = vi()
        wire = tag & 7
        key = tag >> 3
        if key == 0:
            raise ValueError("Invalid protobuf field")
        if wire == 0:
            value = vi()
        elif wire == 2:
            size = vi()
            value = data[i : i + size]
            i += size
            if i > len(data):
                raise ValueError("Truncated protobuf")
        elif wire in (1, 5):
            size = 8 if wire == 1 else 4
            value = data[i : i + size]
            i += size
        else:
            raise ValueError(f"Unsupported wire type {wire}")
        if i > len(data):
            raise ValueError("Truncated protobuf")
        result.setdefault(key, []).append(value)
    return result


def first(data, key, default=None):
    return fields(data).get(key, [default])[0]


def log(event, **kw):
    print(json.dumps({"time": round(time.time(), 3), "event": event, **kw}), flush=True)


class Sender:
    def __init__(self, sock, cert, key):
        self.sock = sock
        self.buf = bytearray()
        self.frags = {}
        self.clear = {}
        self.inbio = ssl.MemoryBIO()
        self.outbio = ssl.MemoryBIO()
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        ctx.maximum_version = ssl.TLSVersion.TLSv1_2
        ctx.load_cert_chain(cert, key)
        self.tls = ctx.wrap_bio(self.inbio, self.outbio, server_side=True)
        self.force_h264 = False
        self.dashboard = False
        self.live = None
        self.touch_forwarder = None
        self.ready = False
        self.video = None
        self.input = None
        self.streaming = False
        self.index = 0
        self.acked = 0
        self.sent = 0
        self.limit = 1
        self.units = []
        self.next_frame = 0
        self.fps = 30
        self.start = time.monotonic()
        self.touch = 0

    def raw(self, ch, flags, data):
        if len(data) > 65535:
            raise ValueError("Frame too large")
        self.sock.sendall(struct.pack("!BBH", ch, flags, len(data)) + data)

    def send(self, ch, msg, payload=b"", encrypted=True, control=False):
        data = struct.pack("!H", msg) + payload
        if not encrypted:
            self.raw(ch, 3, data)
            return
        # TLS record boundaries carry AA fragmentation flags.
        self.tls.write(data)
        encoded = self.outbio.read()
        records = []
        i = 0
        while i < len(encoded):
            size = 5 + int.from_bytes(encoded[i + 3 : i + 5], "big")
            records.append(encoded[i : i + size])
            i += size
        for i, record in enumerate(records):
            flags = (
                8
                | (4 if control else 0)
                | (1 if i == 0 else 0)
                | (2 if i == len(records) - 1 else 0)
            )
            header = struct.pack("!BBH", ch, flags, len(record))
            if flags & 3 == 1:
                header += struct.pack("!I", len(encoded))
            self.sock.sendall(header + record)

    def receive(self):
        chunk = self.sock.recv(65536)
        if not chunk:
            raise EOFError("Headunit disconnected")
        self.buf.extend(chunk)
        while len(self.buf) >= 4:
            ch, flags, n = struct.unpack("!BBH", self.buf[:4])
            extra = 4 if flags & 3 == 1 else 0
            if len(self.buf) < 4 + extra + n:
                return
            data = bytes(self.buf[4 + extra : 4 + extra + n])
            del self.buf[: 4 + extra + n]
            if flags & 8:
                self.inbio.write(data)
                if flags & 1:
                    self.clear[ch] = bytearray()
                while True:
                    try:
                        self.clear.setdefault(ch, bytearray()).extend(
                            self.tls.read(65536)
                        )
                    except ssl.SSLWantReadError:
                        break
                if flags & 2:
                    message = bytes(self.clear.pop(ch, b""))
                    if len(message) >= 2:
                        self.message(
                            ch, int.from_bytes(message[:2], "big"), message[2:]
                        )
            else:
                if flags & 1:
                    self.frags[ch] = bytearray()
                self.frags.setdefault(ch, bytearray()).extend(data)
                if flags & 2:
                    message = bytes(self.frags.pop(ch))
                    self.message(ch, int.from_bytes(message[:2], "big"), message[2:])

    def message(self, ch, msg, data):
        if msg not in (11, 12, 0x8004):
            log("rx", channel=ch, message=hex(msg), size=len(data))
        if ch == 0 and msg == 1:
            log(
                "version",
                major=int.from_bytes(data[:2], "big"),
                minor=int.from_bytes(data[2:4], "big"),
            )
            self.needs_ack = int.from_bytes(data[:2], "big") < 6
            self.send(0, 2, data[:4] + b"\0\0", False)
        elif ch == 0 and msg == 3:
            self.inbio.write(data)
            try:
                self.tls.do_handshake()
                self.ready = True
            except ssl.SSLWantReadError:
                pass
            out = self.outbio.read()
            if out:
                self.send(0, 3, out, False)
            if self.ready:
                log("tls_ready", cipher=self.tls.cipher()[0])
        elif ch == 0 and msg == 4:
            if first(data, 1) != 0:
                raise RuntimeError("Authentication rejected")
            self.send(0, 5, pb(4, "comma lab") + pb(5, "Linux AA test sender"))
        elif ch == 0 and msg == 6:
            (ROOT / ".runtime/service-discovery.bin").write_bytes(data)
            for service in fields(data).get(1, []):
                sf = fields(service)
                sid = sf[1][0]
                if 4 in sf:
                    self.input = sid
                if 3 in sf:
                    sink = fields(sf[3][0])
                    if 4 in sink and first(sf[3][0], 7, 0) == 0:
                        self.video = sid
                        configs = sink[4]
                        for idx, conf in enumerate(configs):
                            if first(conf, 10, first(sf[3][0], 1, 3)) in (3, 7):
                                self.codec = first(conf, 10, first(sf[3][0], 1, 3))
                                res = first(conf, 1, 2)
                                self.fps = 60 if first(conf, 2, 2) == 1 else 30
                                self.size = {
                                    1: (800, 480),
                                    2: (1280, 720),
                                    3: (1920, 1080),
                                }.get(res)
                                if not self.size:
                                    raise RuntimeError(f"Unsupported resolution {res}")
                                self.index = idx
                                break
                        else:
                            raise RuntimeError("No supported video configuration")
            if self.video is None:
                raise RuntimeError("No video channel offered")
            log(
                "services",
                video=self.video,
                input=self.input,
                size=self.size,
                fps=self.fps,
            )
            for channel in (self.video, self.input):
                if channel is not None:
                    self.send(channel, 7, pb(1, 0) + pb(2, channel), control=True)
        elif msg == 8:
            if first(data, 1) != 0:
                raise RuntimeError("Channel open rejected")
            if ch == self.video:
                if self.force_h264:
                    self.codec = 3
                self.send(ch, 0x8000, pb(1, self.codec))
            elif ch == self.input:
                self.send(ch, 0x8002, b"")
        elif ch == self.video and msg == 0x8003:
            if first(data, 1) != 2:
                raise RuntimeError("Video not ready")
            self.limit = first(data, 2, 1)
            self.index = first(data, 3, self.index)
            self.make_video()
            self.send(ch, 0x8001, pb(1, 1) + pb(2, self.index))
            self.send(ch, 0x8007, pb(2, 1))
            self.streaming = True
            self.next_frame = time.monotonic()
            log("stream_start", units=len(self.units))
        elif ch == self.video and msg == 0x8004:
            self.acked += first(data, 2, 1)
        elif ch == 0 and msg == 11:
            self.send(0, 12, pb(1, first(data, 1, 0)))
        elif ch == 0 and msg == 15:
            self.send(0, 16)
            raise EOFError("Headunit requested shutdown")
        elif ch == self.input and msg == 0x8001:
            if self.dashboard:
                from live_video import TouchForwarder

                if self.touch_forwarder is None:
                    self.touch_forwarder = TouchForwarder()

                event = first(data, 3)
                if event:
                    action = first(event, 3, 0)
                    pointer = first(event, 1)
                    if pointer and action in (0, 1, 2):
                        x, y = first(pointer, 1, 0), first(pointer, 2, 0)
                        self.touch_forwarder.submit(x, y, action)
                        log("dashboard_input_queued", x=x, y=y, action=action)
            self.touch += 1
            log(
                "input",
                fields={
                    str(k): [v.hex() if isinstance(v, bytes) else v for v in vs]
                    for k, vs in fields(data).items()
                },
            )

    def make_video(self):
        if self.dashboard:
            from live_video import LiveVideo

            self.live = LiveVideo(self.codec, self.size)
            return
        w, h = self.size
        hevc = self.codec == 7
        p = ROOT / (".runtime/test.h265" if hevc else ".runtime/test.h264")
        codec_args = (
            [
                "-c:v",
                "libx265",
                "-x265-params",
                "aud=1:repeat-headers=1:pools=2:log-level=error",
            ]
            if hevc
            else [
                "-c:v",
                "libx264",
                "-profile:v",
                "baseline",
                "-x264-params",
                "aud=1:repeat-headers=1",
            ]
        )
        subprocess.run(
            [
                "ffmpeg",
                "-hide_banner",
                "-loglevel",
                "error",
                "-y",
                "-f",
                "lavfi",
                "-i",
                f"testsrc2=size={w}x{h}:rate={self.fps}",
                "-t",
                "4",
                *codec_args,
                "-preset",
                "ultrafast",
                "-tune",
                "zerolatency",
                "-pix_fmt",
                "yuv420p",
                "-b:v",
                "1500k",
                "-g",
                str(self.fps),
                "-f",
                "hevc" if hevc else "h264",
                str(p),
            ],
            check=True,
        )
        data = p.read_bytes()
        marker = b"\x00\x00\x00\x01\x46" if hevc else b"\x00\x00\x00\x01\x09"
        starts = []
        pos = 0
        while True:
            pos = data.find(marker, pos)
            if pos < 0:
                break
            starts.append(pos)
            pos += 5
        starts.append(len(data))
        self.units = [data[a:b] for a, b in zip(starts, starts[1:])]
        if not self.units:
            raise RuntimeError("No video access units")

    def tick(self):
        now = time.monotonic()
        if (
            self.streaming
            and now >= self.next_frame
            and (not self.needs_ack or self.sent - self.acked < self.limit)
        ):
            unit = (
                self.live.next()
                if self.live
                else self.units[self.sent % len(self.units)]
            )
            if unit is None:
                return
            self.send(
                self.video, 0, struct.pack("!Q", int((now - self.start) * 1e6)) + unit
            )
            self.sent += 1
            # Keep the negotiated cadence across the receive-loop's small jitter.
            # Rebase after a real stall; never send a burst to catch up old frames.
            self.next_frame = max(self.next_frame + 1 / self.fps, now)
            if self.sent % self.fps == 0:
                log(
                    "video_progress", sent=self.sent, acked=self.acked, touch=self.touch
                )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=45)
    parser.add_argument("--continuous", action="store_true", help="USB service: run until disconnect")
    parser.add_argument("--dashboard", action="store_true")
    parser.add_argument(
        "--host", default="127.0.0.1", help="Explicit AA receiver address"
    )
    parser.add_argument("--port", type=int, default=5277)
    parser.add_argument("--usb", action="store_true", help="Use /dev/usb_accessory instead of TCP")
    parser.add_argument(
        "--h264", action="store_true", help="Request H.264 from the LIVI lab receiver"
    )
    args = parser.parse_args()
    if args.continuous and not args.usb:
        parser.error("continuous mode requires USB")
    if args.continuous:
        import signal
        signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
        os.environ["AA_CONTINUOUS"] = "1"
    if not 0 < args.seconds <= 3600:
        parser.error("seconds must be between 0 and 3600")
    runtime = ROOT / ".runtime"
    runtime.mkdir(exist_ok=True)
    cert = runtime / "aa-test-cert.pem"
    key = runtime / "aa-test-key.pem"
    if not cert.exists():
        subprocess.run(
            [
                "openssl",
                "req",
                "-x509",
                "-newkey",
                "rsa:2048",
                "-nodes",
                "-keyout",
                str(key),
                "-out",
                str(cert),
                "-days",
                "30",
                "-subj",
                "/CN=localhost",
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        key.chmod(0o600)
    if args.usb:
        from usb_transport import AccessoryTransport
        transport = AccessoryTransport("/dev/usb_accessory")
    else:
        transport = socket.create_connection((args.host, args.port), timeout=5)
    with transport as sock:
        sock.settimeout(5)
        s = Sender(sock, str(cert), str(key))
        s.dashboard = args.dashboard
        s.force_h264 = args.h264
        deadline = float("inf") if args.continuous else time.monotonic() + args.seconds
        try:
            while time.monotonic() < deadline:
                if select.select([sock], [], [], 0.005)[0]:
                    s.receive()
                s.tick()
            if s.ready:
                s.send(0, 15, pb(1, 1))
        finally:
            if s.touch_forwarder:
                s.touch_forwarder.close()
            if s.live:
                s.live.close()
            log(
                "summary",
                tls=s.ready,
                stream=s.streaming,
                frames_sent=s.sent,
                frames_acked=s.acked,
                input_events=s.touch,
            )


if __name__ == "__main__":
    main()
