"""Regression using the cached full-rate demo route: left lane change at 204 s."""
import sys,json,bz2,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from dashboard_server import schema
files=json.loads((ROOT/'demo-files.json').read_text())
state='off';tracking=False;offset=0;remaps=0
url=files['logs'][3];raw=bz2.decompress(((ROOT/'cache')/hashlib.sha256(url.split('?')[0].encode()).hexdigest()).read_bytes())
for e in sorted(schema.Event.read_multiple_bytes(raw),key=lambda e:e.logMonoTime):
 if e.which()=='lateralPlanDEPRECATED':state=str(e.lateralPlanDEPRECATED.laneChangeState)
 if e.which()!='modelV2':continue
 m=e.modelV2
 if state not in ['laneChangeStarting','laneChangeFinishing']:tracking=False;continue
 left,right=m.laneLines[1].y[0],m.laneLines[2].y[0];width=right-left;center=(left+right)/2
 if not (2<width<5 and min(m.laneLineProbs[1],m.laneLineProbs[2])>.02):tracking=False;continue
 if not tracking:start=offset;origin=center;reference=width;peak=0;remapped=False
 progress=center-origin;peak=max(peak,progress)
 if not remapped and peak-progress>reference*.35:remapped=True;remaps+=1
 if remapped:progress+=reference
 offset=start-max(0,min(reference*1.25,progress))
 prev=center;tracking=True
print('Measured lateral displacement:',round(offset,2),'m; lane-index remaps:',remaps)
assert -4.5<offset<-2
