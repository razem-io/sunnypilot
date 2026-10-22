"""Incremental Annex-B encoder output; no prerecorded dashboard frames."""

from collections import deque
import os
import select
import subprocess
import urllib.request
from pathlib import Path
import struct


class LiveVideo:
    def __init__(self, codec, size):
        if tuple(size) != (1280, 720):
            raise RuntimeError("Dashboard bridge currently requires 1280x720")
        self.hardware = os.environ.get("AA_ENCODER") == "qcom"
        if self.hardware:
            if codec != 3:
                raise RuntimeError("Qualcomm dashboard experiment currently requires H.264")
            self.feed = None
            self.buffer = bytearray()
            self.frames = deque()
            self.process = subprocess.Popen(
                [str(Path(__file__).resolve().parents[1] / "qcom-encoder-probe"),
                 "60", "0" if os.environ.get("AA_CONTINUOUS") == "1" else "600", "-", "nv12", os.environ.get("AA_FRAME_SOURCE", "shared")], stdout=subprocess.PIPE,
                stdin=subprocess.DEVNULL)
            return
        try:
            from .yuv_feed import YuvFeed, load_converter
        except ImportError:
            from yuv_feed import YuvFeed, load_converter
        converter = load_converter()
        self.feed = None
        hevc = codec == 7
        self.marker = b"\0\0\0\1\x46" if hevc else b"\0\0\0\1\x09"
        self.buffer = bytearray()
        self.frames = deque()
        encoder = (
            [
                "-c:v",
                "libx265",
                "-x265-params",
                "aud=1:repeat-headers=1:pools=2:log-level=error",
            ]
            if hevc
            else ["-c:v", "libx264", "-x264-params", "aud=1:repeat-headers=1"]
        )
        self.process = subprocess.Popen(
            [
                "ffmpeg",
                "-hide_banner",
                "-loglevel",
                "error",
                "-analyzeduration",
                "100000",
                "-probesize",
                "32768",
                "-f",
                "rawvideo",
                "-pixel_format",
                "yuv420p" if converter else "bgra",
                "-video_size",
                "1280x720" if converter else "1280x640",
                "-framerate",
                "30",
                "-i",
                "pipe:0" if converter else "http://127.0.0.1:8779/raw",
                "-filter_threads",
                "2",
                "-vf",
                "null" if converter else "pad=1280:720:0:40:black",
                *encoder,
                "-threads",
                "2",
                "-preset",
                "ultrafast",
                "-tune",
                "zerolatency",
                "-pix_fmt",
                "yuv420p",
                "-b:v",
                "3000k",
                "-g",
                "30",
                "-flush_packets",
                "1",
                "-f",
                "hevc" if hevc else "h264",
                "pipe:1",
            ],
            stdout=subprocess.PIPE,
            stdin=subprocess.PIPE if converter else subprocess.DEVNULL,
        )

        if converter:
            self.feed = YuvFeed(self.process.stdin, converter)

    def next(self):
        if select.select([self.process.stdout], [], [], 0)[0]:
            data = os.read(self.process.stdout.fileno(), 65536)
            if not data:
                raise RuntimeError("Dashboard encoder stopped")
            self.buffer.extend(data)
            while True:
                if getattr(self, "hardware", False):
                    if len(self.buffer) < 4:
                        break
                    size = struct.unpack_from("!I", self.buffer)[0]
                    if not 0 < size <= 4 * 1024 * 1024:
                        raise RuntimeError("Invalid hardware video packet size")
                    if len(self.buffer) < 4 + size:
                        break
                    self.frames.append(bytes(self.buffer[4:4 + size]))
                    del self.buffer[:4 + size]
                    continue
                start = self.buffer.find(self.marker)
                end = (
                    self.buffer.find(self.marker, start + len(self.marker))
                    if start >= 0
                    else -1
                )
                if end < 0:
                    break
                self.frames.append(bytes(self.buffer[:end]))
                del self.buffer[:end]
            if len(self.frames) > 60 or len(self.buffer) > 8 * 1024 * 1024:
                raise RuntimeError("Dashboard encoder backlog exceeded")
        return self.frames.popleft() if self.frames else None

    def close(self):
        if self.feed:
            self.feed.stopped.set()
        self.process.terminate()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.process.stdout.close()
        if self.feed:
            self.feed.close()


def forward_touch(x, y, action):
    # This endpoint belongs to our isolated native lab app, not the OS desktop.
    with urllib.request.urlopen(
        f"http://127.0.0.1:8779/input?x={x}&y={y}&action={action}", timeout=2.0
    ) as response:
        response.read()


class TouchForwarder:
    """Keep slow device HTTP handling off the video loop; coalesce drag moves."""

    def __init__(self):
        import threading

        self.condition = threading.Condition()
        self.pending = deque()
        self.stopped = False
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def submit(self, x, y, action):
        with self.condition:
            if self.stopped:
                return
            if action == 2 and self.pending and self.pending[-1][2] == 2:
                self.pending[-1] = (x, y, action)
            else:
                if len(self.pending) >= 64:
                    self.pending.clear()
                    self.pending.append((x, y, 1))
                self.pending.append((x, y, action))
            self.condition.notify()

    def _run(self):
        while True:
            with self.condition:
                self.condition.wait_for(lambda: self.pending or self.stopped)
                if self.stopped:
                    return
                event = self.pending.popleft()
            try:
                forward_touch(*event)
            except OSError:
                pass  # A delayed input must never tear down the video session.

    def close(self):
        with self.condition:
            self.stopped = True
            self.pending.clear()
            self.condition.notify()
        self.thread.join(timeout=3)
        try:
            forward_touch(640, 360, 1)  # Release an interrupted drag.
        except OSError:
            pass
