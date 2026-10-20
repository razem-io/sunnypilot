"""Local-only adapter: same display policy as the native dashboard, no CAN publishing."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'sunnypilot/custom/niro_dashboard'))
from policy import eligible
from telemetry import readings, finite
REQUIRED=['deviceState','carState','carControl','selfdriveState','selfdriveStateSP']
SCENARIOS=['manual','park','reverse','reverse_alert','left','right','hazards','brake','door','seatbelt','blindspot','assist','lateral','longitudinal','mads','alert','stale','invalid_can']
def fixture(name):
 if name not in SCENARIOS:raise ValueError('Unknown scenario')
 d={'deviceState':{'started':True},'carState':{'canValid':True,'vEgo':15.,'steeringAngleDeg':90.,'gearShifter':'drive','leftBlinker':False,'rightBlinker':False,'doorOpen':False,'seatbeltUnlatched':False,'brakePressed':False,'gasPressed':False,'leftBlindspot':False,'rightBlindspot':False,'wheelSpeeds':{'fl':15.,'fr':15.,'rl':15.,'rr':15.}},'carControl':{'enabled':False,'latActive':False,'longActive':False},'selfdriveState':{'state':'disabled','enabled':False,'active':False,'alertSize':'none','alertText1':'','alertText2':''},'selfdriveStateSP':{'mads':{'state':'disabled','enabled':False,'active':False}},'carParams':{'steerRatio':15.,'carFingerprint':'KIA NIRO EV · TESTDATEN'}}
 f={k:True for k in d}
 c=d['carState'];ss=d['selfdriveState'];cc=d['carControl']
 if name=='park':d['deviceState']['started']=False;c.update(vEgo=0,gearShifter='park')
 if name.startswith('reverse'):c.update(vEgo=1.5,gearShifter='reverse')
 if name in ('left','hazards'):c['leftBlinker']=True
 if name in ('right','hazards'):c['rightBlinker']=True
 if name=='brake':c['brakePressed']=True
 if name=='door':c['doorOpen']=True
 if name=='seatbelt':c['seatbeltUnlatched']=True
 if name=='blindspot':c['leftBlindspot']=True
 if name=='assist':ss.update(state='enabled',enabled=True,active=True);cc['enabled']=True
 if name=='lateral':cc['latActive']=True
 if name=='longitudinal':cc['longActive']=True
 if name=='mads':d['selfdriveStateSP']['mads'].update(state='enabled',enabled=True,active=True)
 if name in ('alert','reverse_alert'):ss.update(alertSize='full',alertText1='Rückwärtsgang' if name=='reverse_alert' else 'Test-Warnmeldung')
 if name=='stale':f['carState']=False
 if name=='invalid_can':c['canValid']=False
 return d,f

def snapshot(d,f):
 car=d.get('carState',{});ratio=d.get('carParams',{}).get('steerRatio')
 ratio_source='carParams' if finite(ratio) and 1 < ratio < 100 else None
 lp=d.get('liveParameters',{})
 if ratio_source is None and f.get('liveParameters',False) and lp.get('valid') and lp.get('steerRatioValid') and finite(lp.get('steerRatio')) and 1 < lp['steerRatio'] < 100:
  ratio=lp['steerRatio'];ratio_source='liveParameters'
 values=readings(car,f.get('carState',False),ratio)
 missing=[k for k in REQUIRED if not f.get(k,False)]
 show=eligible(d,f)
 # The native eligibility policy is preserved exactly; validity of displayed
 # sensor values is a separate concern.
 ss=d.get('selfdriveState',{});cc=d.get('carControl',{});mads=d.get('selfdriveStateSP',{}).get('mads',{})
 if missing:reason='Fehlende/veraltete Dienste: '+', '.join(missing)
 elif ss.get('alertSize','none')!='none' or ss.get('alertText1') or ss.get('alertText2'):reason='Warnmeldung hat Vorrang'
 elif any([ss.get('enabled'),ss.get('active'),cc.get('enabled'),cc.get('latActive'),cc.get('longActive'),mads.get('enabled'),mads.get('active')]):reason='Fahrassistenz aktiv'
 else:reason='Manuell / geparkt' if show else 'Zustand nicht freigegeben'
 return {**values,'show':show,'reason':reason,'missing':missing,'gear':car.get('gearShifter') if values['live'] else None,'car':car if values['live'] else {},'raw':{k:v for k,v in d.items() if k not in ('modelV2','roadCameraState')},'fresh':f,'fingerprint':d.get('carParams',{}).get('carFingerprint','Unbekannt'),'ratio':ratio,'ratioSource':ratio_source}
