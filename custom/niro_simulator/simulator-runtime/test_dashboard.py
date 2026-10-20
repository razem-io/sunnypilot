import unittest
from dashboard_state import fixture, snapshot, SCENARIOS
from niro_doors import decode_doors, DOORS

class DashboardTests(unittest.TestCase):
    def test_transition_sequence(self):
        for _ in range(100):
            for case, shown in [('manual', True), ('reverse', True), ('reverse_alert', False), ('manual', True), ('assist', False), ('manual', True), ('stale', False), ('park', True)]:
                with self.subTest(case=case):
                    self.assertEqual(snapshot(*fixture(case))['show'], shown)
    def test_every_assistance_and_warning_yields(self):
        for case in ('assist','lateral','longitudinal','mads','alert','reverse_alert'):
            with self.subTest(case=case): self.assertFalse(snapshot(*fixture(case))['show'])
    def test_missing_required_channels(self):
        for channel in ('deviceState','carState','carControl','selfdriveState','selfdriveStateSP'):
            d,f=fixture('manual'); f[channel]=False
            self.assertFalse(snapshot(d,f)['show'])
    def test_invalid_values_are_not_demo_or_zero(self):
        for case in ('stale','invalid_can'):
            s=snapshot(*fixture(case))
            for k in ('speed','steering','wheelAngle','gear'): self.assertIsNone(s[k])
            self.assertEqual(s['car'],{})
    def test_live_units_and_wheel_sign(self):
        d,f=fixture('manual');d['carState'].update(vEgo=10,steeringAngleDeg=-150)
        s=snapshot(d,f);self.assertEqual((s['speed'],s['steering'],s['wheelAngle']),(36,-150,-10))
        d['carParams']['steerRatio']=None
        self.assertIsNone(snapshot(d,f)['wheelAngle'])
    def test_ratio_fallback_requires_valid_fresh_estimate(self):
        d,f=fixture('manual');d.pop('carParams')
        d['liveParameters']={'valid':True,'steerRatioValid':True,'steerRatio':18};f['liveParameters']=True
        self.assertEqual(snapshot(d,f)['wheelAngle'],5)
        self.assertEqual(snapshot(d,f)['ratioSource'],'liveParameters')
        f['liveParameters']=False
        self.assertIsNone(snapshot(d,f)['wheelAngle'])
    def test_nonfinite(self):
        d,f=fixture('manual');d['carState'].update(vEgo=float('nan'),steeringAngleDeg=float('inf'))
        s=snapshot(d,f);self.assertIsNone(s['speed']);self.assertIsNone(s['wheelAngle'])
    def test_signals_and_full_carstate_preserved(self):
        for case,field in [('left','leftBlinker'),('right','rightBlinker'),('brake','brakePressed'),('door','doorOpen'),('seatbelt','seatbeltUnlatched'),('blindspot','leftBlindspot')]:
            s=snapshot(*fixture(case));self.assertTrue(s['car'][field]);self.assertTrue(s['show'])
        d,f=fixture('manual');d['carState']['futureSensor']={'value':123}
        self.assertEqual(snapshot(d,f)['raw']['carState']['futureSensor']['value'],123)
    def test_fixtures_are_isolated(self):
        for name in SCENARIOS:
            d,f=fixture(name);d['carState']['vEgo']=999
        self.assertEqual(fixture('manual')[0]['carState']['vEgo'],15)

class DoorTests(unittest.TestCase):
    def frames(self, name=None, value=1):
        frames={1345:(bytes(8),10),1363:(bytes(8),10)}
        if name:
            address,shift,width=DOORS[name];frames[address]=((value<<shift).to_bytes(8,'little'),10)
        return frames
    def test_all_four_independent(self):
        for name in DOORS:
            result=decode_doors(self.frames(name),niro_ev_verified=True,now=10.1)
            self.assertTrue(result['complete']);self.assertEqual([k for k,v in result['doors'].items() if v],[name])
    def test_unknown_vehicle_no_decoding(self):
        self.assertTrue(all(v is None for v in decode_doors(self.frames(),niro_ev_verified=False,now=10)['doors'].values()))
    def test_missing_stale_short_future_frames(self):
        for frames in ({},self.frames(),{1345:(b'\x00',10)},{1345:(bytes(8),20)}):
            self.assertFalse(decode_doors(frames,niro_ev_verified=True,now=11)['complete'])
    def test_reserved_driver_value(self):
        for value in (2,3):
            s=decode_doors(self.frames('frontLeft',value),niro_ev_verified=True,now=10)
            self.assertIsNone(s['doors']['frontLeft']);self.assertEqual(s['raw']['frontLeft'],value)

if __name__=='__main__':unittest.main(verbosity=2)
