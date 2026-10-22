import importlib.util
from pathlib import Path
import sys
import time
import unittest

spec = importlib.util.spec_from_file_location('usb_autoconnect', Path(__file__).resolve().parents[1] / 'projection/device/usb_autoconnect.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class AutoconnectTest(unittest.TestCase):
    def make_session(self, code='import time; time.sleep(60)', ready=lambda: True):
        session = module.Session([sys.executable, '-u', '-c', code], ready=ready)
        self.addCleanup(session.selector.close)
        self.addCleanup(session.stop)
        return session

    def test_unplug_stops_and_replug_starts_only_one_sender(self):
        session = self.make_session()
        session.tick(False, 0)
        self.assertIsNone(session.process)
        session.tick(True, 1)
        old = session.process
        session.tick(True, 2)
        self.assertIs(old, session.process)
        session.tick(False, 3)
        self.assertIsNotNone(old.poll())
        self.assertIsNone(session.process)
        session.tick(True, 4)
        self.assertNotEqual(old.pid, session.process.pid)

    def test_waits_for_ui_and_retries_stalled_sender(self):
        ready = [False]
        session = self.make_session(ready=lambda: ready[0])
        session.tick(True, 0)
        self.assertIsNone(session.process)
        ready[0] = True
        session.tick(True, 2)
        old = session.process
        session.tick(True, 28)
        self.assertIsNotNone(old.poll())
        self.assertIsNone(session.process)
        session.tick(True, 31)
        self.assertIsNotNone(session.process)

    def test_failed_sender_uses_backoff(self):
        session = self.make_session('raise SystemExit(1)')
        session.tick(True, 1)
        session.process.wait(timeout=3)
        session.tick(True, 2)
        self.assertIsNone(session.process)
        session.tick(True, 3)
        self.assertIsNone(session.process)
        session.tick(True, 4)
        self.assertIsNotNone(session.process)
