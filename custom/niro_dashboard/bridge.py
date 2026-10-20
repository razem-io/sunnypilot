import json,os,signal,time,struct,sys
from cereal import messaging, car as car_schema
from common.params import Params
from telemetry import readings
from policy import eligible
sm=messaging.SubMaster(['deviceState','carState','carControl','selfdriveState','selfdriveStateSP'])
last={k:0. for k in sm.services};parent=os.getppid();heartbeat=f'/dev/shm/niro-dashboard-{parent}';start=time.monotonic()
ui_pid=None;ui_scan=0
params=Params();ratio=None;ratio_scan=-5
def stock_ui_ready(now):
  global ui_pid,ui_scan
  if now-ui_scan>1:
    ui_scan=now;ui_pid=None
    for name in os.listdir('/proc'):
      if not name.isdigit():continue
      try:
        if os.readlink('/proc/'+name+'/exe').endswith('/selfdrive/ui/ui'):ui_pid=name;break
      except OSError:pass
  try:
    with open('/dev/shm/wd_'+str(ui_pid),'rb') as f:stamp=struct.unpack('<Q',f.read(8))[0]/1e9
    return any(0<=clock-stamp<3 for clock in (now,time.clock_gettime(time.CLOCK_BOOTTIME)))
  except (OSError,struct.error):return False
def display_awake():
  try:
    with open('/sys/class/backlight/panel0-backlight/brightness') as f:return int(f.read())>0
  except (OSError,ValueError):return False
while True:
  sm.update(50);now=time.monotonic()
  for k in sm.services:
    if sm.updated[k]:last[k]=now
  fresh={k:bool(sm.valid[k] and now-last[k]<(3 if k=='deviceState' else .5)) for k in sm.services}
  data={k:sm[k].to_dict() for k in sm.services}
  car=data['carState']
  if now-ratio_scan>5:
    ratio_scan=now;ratio=None
    try:
      raw=params.get('CarParams')
      if raw:
        with car_schema.CarParams.from_bytes(raw) as cp:ratio=cp.steerRatio
    except Exception as e:
      print('CarParams unavailable: '+type(e).__name__,file=sys.stderr,flush=True)
  values=readings(car,fresh['carState'],ratio);live=values['live']
  print(json.dumps({'show':eligible(data,fresh) and stock_ui_ready(now) and display_awake(),'parked':not data['deviceState'].get('started',True),**values,'gear':car.get('gearShifter','unknown') if live else 'unknown'}),flush=True)
  try:
    age=time.time()-os.stat(heartbeat).st_mtime
    # Only the first graphics initialization while definitely parked gets a
    # bounded grace period. Onroad, invalid state, alerts: keep the 1s deadline.
    warm=False
    if fresh['deviceState'] and not data['deviceState'].get('started',True) and eligible(data,fresh):
      try:warm=0<=time.time()-os.stat(heartbeat+'.warming').st_mtime<5
      except FileNotFoundError:pass
    if now-start>3 and age>1 and not warm:
      print(f'WATCHDOG: terminate stalled dashboard (heartbeat {age:.2f}s)',file=sys.stderr,flush=True)
      os.kill(parent,signal.SIGKILL);break
  except FileNotFoundError:
    if now-start>3:
      print('WATCHDOG: terminate dashboard without heartbeat',file=sys.stderr,flush=True)
      os.kill(parent,signal.SIGKILL);break
