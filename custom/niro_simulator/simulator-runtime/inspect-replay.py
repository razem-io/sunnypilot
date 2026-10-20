"""Read-only observer of the official local Replay; never publishes messages."""
from pathlib import Path
import json,time
import capnp,zmq
root=Path(__file__).resolve().parent
schema=capnp.load(str(root/'source/cereal/log.capnp'),imports=['/opt/homebrew/include',str(root/'source/cereal')])
def port(name):
 h=0xcbf29ce484222325
 for c in name.encode():h=((h^c)*0x100000001b3)&((1<<64)-1)
 return 8023+h%(65535-8023)
c=zmq.Context();s=c.socket(zmq.SUB);s.setsockopt(zmq.SUBSCRIBE,b'');s.setsockopt(zmq.CONFLATE,1);s.connect(f'tcp://127.0.0.1:{port("carState")}')
count=0;end=time.monotonic()+8
while time.monotonic()<end and count<3:
 if not s.poll(1000):continue
 with schema.Event.from_bytes(s.recv()) as e:
  cs=e.carState
  print(json.dumps({'valid':e.valid,'speed_kmh':round(cs.vEgo*3.6,1),'steering_degrees':round(cs.steeringAngleDeg,1),'gear':str(cs.gearShifter),'leftBlinker':cs.leftBlinker,'rightBlinker':cs.rightBlinker,'doorOpen':cs.doorOpen}),flush=True)
 count+=1;time.sleep(.5)
s.close();c.term()
if not count:raise SystemExit('No carState received from local replay')
