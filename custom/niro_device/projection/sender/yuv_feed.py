"""SIMD BGRA -> I420 feed; bounded buffers and natural encoder backpressure."""
import ctypes
import ctypes.util
import os
from pathlib import Path
import threading
import urllib.request


def load_converter():
    local = Path(__file__).resolve().parents[1] / 'libyuv.so.0'
    path = os.environ.get('AA_LIBYUV') or (str(local) if local.exists() else ctypes.util.find_library('yuv'))
    if not path:
        return None
    lib = ctypes.CDLL(path)
    convert = lib.ARGBToI420  # libyuv names are register order; Qt memory is BGRA.
    convert.argtypes = [ctypes.c_void_p, ctypes.c_int] * 4 + [ctypes.c_int] * 2
    convert.restype = ctypes.c_int
    return convert


class YuvFeed:
    def __init__(self, pipe, convert):
        self.pipe = pipe
        self.convert = convert
        self.stopped = threading.Event()
        self.error = None
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        width, height, padded = 1280, 640, 720
        rgba = bytearray(width * height * 4)
        src = (ctypes.c_ubyte * len(rgba)).from_buffer(rgba)
        # Limited-range black bars, with a centered 1280x640 active picture.
        out = bytearray([16]) * (width * padded) + bytearray([128]) * (width * padded // 2)
        dst = (ctypes.c_ubyte * len(out)).from_buffer(out)
        base = ctypes.addressof(dst)
        y = base + width * 40
        u = base + width * padded + (width // 2) * 20
        v = base + width * padded * 5 // 4 + (width // 2) * 20
        try:
            with urllib.request.urlopen('http://127.0.0.1:8779/raw', timeout=3) as response:
                while not self.stopped.is_set():
                    offset = 0
                    while offset < len(rgba) and not self.stopped.is_set():
                        n = response.readinto(memoryview(rgba)[offset:])
                        if not n:
                            raise EOFError('Dashboard raw stream ended')
                        offset += n
                    if self.stopped.is_set():
                        break
                    result = self.convert(src, width * 4, y, width, u, width // 2, v, width // 2, width, height)
                    if result:
                        raise RuntimeError(f'libyuv conversion failed: {result}')
                    self.pipe.write(out)
                    self.pipe.flush()
        except (OSError, EOFError, RuntimeError, ValueError) as exc:
            if not self.stopped.is_set():
                self.error = exc
        finally:
            try:
                self.pipe.close()
            except OSError:
                pass

    def close(self):
        self.stopped.set()
        self.thread.join(timeout=4)
