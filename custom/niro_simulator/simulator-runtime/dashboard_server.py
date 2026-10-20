from pathlib import Path
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
import threading,json,time,os,math,sys
import capnp,zmq
from dashboard_state import snapshot,fixture,SCENARIOS
ROOT=Path(__file__).resolve().parent;PROJECT=ROOT.parent
schema=capnp.load(str(ROOT/'source/cereal/log.capnp'),imports=['/opt/homebrew/include',str(ROOT/'source/cereal')])
SERVICES=['carState','carControl','deviceState','selfdriveState','selfdriveStateSP','controlsState','carParams','liveParameters','modelV2','roadCameraState']
lock=threading.Lock();messages={};received={};valid={};mode='replay';case='manual';overrides={};history=[];last_view=None

def port(name):
 h=0xcbf29ce484222325
 for c in name.encode():h=((h^c)*0x100000001b3)&((1<<64)-1)
 return 8023+h%(65535-8023)
def reader():
 ctx=zmq.Context();poll=zmq.Poller();sockets={}
 for name in SERVICES:
  s=ctx.socket(zmq.SUB);s.setsockopt(zmq.SUBSCRIBE,b'');s.setsockopt(zmq.CONFLATE,1);s.connect(f'tcp://127.0.0.1:{port(name)}');poll.register(s,zmq.POLLIN);sockets[s]=name
 while True:
  for s,_ in poll.poll(200):
   try:
    with schema.Event.from_bytes(s.recv(),traversal_limit_in_words=2**25) as e:
     name=sockets[s];payload=getattr(e,name).to_dict()
     with lock:messages[name]=payload;received[name]=time.monotonic();valid[name]=e.valid
   except Exception as e:print('Reader:',type(e).__name__,flush=True)
def clean(x):
 if isinstance(x,float) and not math.isfinite(x):return None
 if isinstance(x,bytes):return f'<{len(x)} bytes>'
 if isinstance(x,dict):return {k:clean(v) for k,v in x.items()}
 if isinstance(x,list):return [clean(v) for v in x]
 return x
def state():
 global last_view
 with lock:
  now=time.monotonic();d=dict(messages);f={k:valid.get(k,False) and now-received.get(k,-100)<(3 if k=='deviceState' else 30 if k=='carParams' else .5) for k in SERVICES}
  source=mode;testcase=case
  if mode=='test':
   d,f=fixture(case);d['carState'].update(overrides)
  out=snapshot(d,f);out.update(source=source,scenario=testcase,ages={k:round(now-v,2) for k,v in received.items()},scenarios=SCENARIOS)
  signature=(source,out['show'],out['reason'])
  if signature!=last_view:
   last_view=signature;history.append({'time':time.strftime('%H:%M:%S'),'source':source,'show':out['show'],'reason':out['reason']});del history[:-30]
  out['history']=list(reversed(history));return clean(out)
class Handler(SimpleHTTPRequestHandler):
 def __init__(self,*a,**kw):super().__init__(*a,directory=str(PROJECT),**kw)
 def log_message(self,*a):pass
 def send_json(self,obj,status=200):
  b=json.dumps(obj,allow_nan=False).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)
 def do_GET(self):
  if self.path=='/api/state':return self.send_json(state())
  if self.path=='/':self.path='/simulator-runtime/dashboard/index.html'
  return super().do_GET()
 def do_POST(self):
  global mode,case,overrides
  if self.path!='/api/source':return self.send_json({'error':'Not found'},404)
  if self.headers.get('Origin') not in (None,'http://127.0.0.1:8774','http://localhost:8774'):return self.send_json({'error':'Origin rejected'},403)
  try:
   n=int(self.headers.get('Content-Length',0));assert 0<n<4096
   obj=json.loads(self.rfile.read(n));assert obj.get('source') in ('replay','test')
   assert obj.get('scenario','manual') in SCENARIOS
   ov=obj.get('values',{});assert set(ov)<= {'vEgo','steeringAngleDeg'}
   assert all(isinstance(v,(float,int)) and math.isfinite(v) for v in ov.values())
   assert 0<=ov.get('vEgo',0)<=60 and abs(ov.get('steeringAngleDeg',0))<=540
   with lock:mode=obj['source'];case=obj.get('scenario','manual');overrides=ov
   return self.send_json(state())
  except (ValueError,AssertionError,TypeError):return self.send_json({'error':'Invalid input'},400)
if __name__=='__main__':
 threading.Thread(target=reader,daemon=True).start()
 print('Replay dashboard: http://127.0.0.1:8774',flush=True)
 ThreadingHTTPServer(('127.0.0.1',8774),Handler).serve_forever()
