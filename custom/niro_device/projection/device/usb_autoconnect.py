"""Start one GPU sender when a USB host configures the accessory; recover on reconnect."""
import os
from pathlib import Path
import selectors
import signal
import subprocess
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
STATE = Path('/sys/class/udc/a600000.dwc3/state')


def bridge_ready():
    try:
        with urllib.request.urlopen('http://127.0.0.1:8779/stats', timeout=1) as response:
            return response.status == 200
    except OSError:
        return False


def stop_group(process):
    # Kill the sender's whole group, including any encoder left by a crash.
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        pass
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()
    process.stdout.close()


class Session:
    def __init__(self, command, ready=bridge_ready):
        self.command, self.ready = command, ready
        self.process = None
        self.selector = selectors.DefaultSelector()
        self.pending = b''
        self.last_progress = 0
        self.retry_at = 0
        self.failures = 0
        self.started_at = 0
        self.last_report = 0

    def stop(self):
        if self.process:
            self.selector.unregister(self.process.stdout)
            stop_group(self.process)
            self.process = None
            self.pending = b''

    def tick(self, connected, now):
        if not connected:
            if self.process:
                print('USB disconnected; stopping projection', flush=True)
            self.stop()
            self.failures = 0
            self.retry_at = 0
            return
        if self.process:
            for key, _ in self.selector.select(0):
                data = os.read(key.fd, 65536)
                self.pending += data
                while b'\n' in self.pending:
                    line, self.pending = self.pending.split(b'\n', 1)
                    text = line.decode(errors='replace')
                    progress = 'video_progress' in text
                    if progress:
                        self.last_progress = now
                        if now - self.started_at > 30:
                            self.failures = 0
                    # Preserve errors/handshake; rate-limit continuous frame diagnostics.
                    if text.startswith('progress sent='):
                        continue
                    if not progress or now - self.last_report >= 30:
                        print(text, flush=True)
                        if progress:
                            self.last_report = now
                if len(self.pending) > 65536:
                    self.pending = b''
            if self.process.poll() is not None or now - self.last_progress > 25:
                print('Projection ended or stalled; retrying', flush=True)
                self.stop()
                self.failures += 1
                self.retry_at = now + min(30, 2 ** min(self.failures, 5))
        if not self.process and now >= self.retry_at:
            if not self.ready():
                self.retry_at = now + 2
                return
            print('USB host ready; starting Android Auto dashboard', flush=True)
            self.process = subprocess.Popen(self.command, cwd=ROOT,
                env={**os.environ, 'AA_ENCODER': 'qcom', 'AA_FRAME_SOURCE': 'shared', 'PYTHONUNBUFFERED': '1'},
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
            os.set_blocking(self.process.stdout.fileno(), False)
            self.selector.register(self.process.stdout, selectors.EVENT_READ)
            self.last_progress = self.started_at = now


def main():
    stopping = False
    def stop(*_):
        nonlocal stopping
        stopping = True
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    session = Session(['python3', '-u', str(ROOT / 'sender/aa_sender.py'),
                       '--usb', '--h264', '--dashboard', '--continuous'])
    print('Waiting for USB headunit; automatic reconnect enabled', flush=True)
    try:
        while not stopping:
            try:
                connected = STATE.read_text().strip() == 'configured'
            except OSError:
                connected = False
            session.tick(connected, time.monotonic())
            time.sleep(.5)
    finally:
        session.stop()
        session.selector.close()


if __name__ == '__main__':
    main()
