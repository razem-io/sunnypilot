"""Native Qt stdin bridge: directly subscribe to original Replay, no HTTP/HTML."""
from pathlib import Path
import sys,threading,time,json,os
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import dashboard_server as replay
from dashboard_state import snapshot
threading.Thread(target=replay.reader,daemon=True).start()
while True:
    s=replay.state()
    # Optional explicit local-only legacy-recording test assumption. Never sent
    # into ZMQ, never used by the vehicle deployment bridge.
    if os.environ.get('NIRO_REPLAY_ASSUME_MADS_OFF')=='1' and not s['fresh'].get('selfdriveStateSP'):
        d=s['raw'];f=s['fresh'];d['selfdriveStateSP']={'mads':{'state':'disabled','enabled':False,'active':False}};f['selfdriveStateSP']=True
        s.update(snapshot(d,f));s['reason']='LOKALER TEST: fehlendes MADS als aus angenommen · '+s['reason']
    s['parked']=not s['raw'].get('deviceState',{}).get('started',True)
    print(json.dumps({k:v for k,v in s.items() if k not in ('raw','car','history')}),flush=True)
    time.sleep(.05)
