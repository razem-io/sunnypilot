import unittest
from telemetry import readings

class TelemetryTests(unittest.TestCase):
  def test_units_and_sign(self):
    r = readings({'canValid': True, 'vEgo': 20, 'steeringAngleDeg': -90}, True, 15)
    self.assertEqual((r['speed'], r['steering'], r['wheelAngle']), (72, -90, -6))
  def test_invalid_and_stale(self):
    for fresh, valid in [(False, True), (True, False)]:
      r = readings({'canValid': valid, 'vEgo': 20, 'steeringAngleDeg': 90}, fresh, 15)
      self.assertIsNone(r['speed']); self.assertIsNone(r['steering']); self.assertIsNone(r['wheelAngle'])
  def test_missing_ratio_and_nonfinite(self):
    r = readings({'canValid': True, 'vEgo': float('nan'), 'steeringAngleDeg': 0}, True)
    self.assertIsNone(r['speed']); self.assertEqual(r['steering'], 0); self.assertIsNone(r['wheelAngle'])
