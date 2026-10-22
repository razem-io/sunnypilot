"""Socket-like Android accessory transport for the device-side lab sender."""
import os
import socket
import threading


class AccessoryTransport:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR)
        self.reader, self.writer = socket.socketpair()
        self.thread = threading.Thread(target=self._read, daemon=True)
        self.thread.start()

    def _read(self):
        try:
            while True:
                data = os.read(self.fd, 16384)
                if not data:
                    break
                self.writer.sendall(data)
        except OSError:
            pass
        finally:
            self.writer.close()

    def fileno(self):
        return self.reader.fileno()

    def settimeout(self, value):
        self.reader.settimeout(value)

    def recv(self, count):
        return self.reader.recv(count)

    def sendall(self, data):
        view = memoryview(data)
        while view:
            count = os.write(self.fd, view[:16384])
            if count <= 0:
                raise OSError('Accessory write failed')
            view = view[count:]

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.reader.close()
        os.close(self.fd)
