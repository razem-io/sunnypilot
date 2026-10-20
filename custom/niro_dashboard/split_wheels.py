"""Split complete welded wheel components; never cut triangles or deform body panels.
Input is the normalized niro.mesh from convert_model.py. Material bits 2/3
encode the left/right front wheel. Original mesh remains untouched.
"""
from pathlib import Path
import struct
import numpy as np
root=Path(__file__).parent/'assets'
parts=[];totals=[0,0,0]
with (root/'niro.mesh').open('rb') as f:
  count=struct.unpack('<I',f.read(4))[0]
  for _ in range(count):
    nv,ni,mat=struct.unpack('<III',f.read(12))
    v=np.frombuffer(f.read(nv*32),'<f4').reshape(-1,8)
    idx=np.frombuffer(f.read(ni*4),'<u4').reshape(-1,3)
    _,inv=np.unique(np.round(v[:,:3],5),axis=0,return_inverse=True)
    parent=list(range(inv.max()+1))
    def find(a):
      while parent[a]!=a:parent[a]=parent[parent[a]];a=parent[a]
      return a
    for tri in inv[idx]:
      a=find(tri[0]);parent[find(tri[1])]=a;parent[find(tri[2])]=a
    groups={}
    for ti,t in enumerate(idx):groups.setdefault(find(inv[t[0]]),[]).append(ti)
    tags=np.zeros(len(idx),dtype=int)
    for ts in groups.values():
      p=v[idx[ts].flatten(),:3]
      radius=(p[:,0]+1.294)**2+(p[:,1]-.3285)**2
      if radius.max()<.34**2 and np.abs(p[:,2]).min()>.68 and np.abs(p[:,2]).max()<.90:
        if np.all(p[:,2]>0):tags[ts]=1
        elif np.all(p[:,2]<0):tags[ts]=2
    for tag in range(3):
      faces=idx[tags==tag]
      if not len(faces):continue
      used,reindex=np.unique(faces,return_inverse=True)
      parts.append((v[used],reindex.astype('<u4').ravel(),mat+tag*4));totals[tag]+=len(faces)
assert totals[1]>10000 and totals[2]>10000,totals
with (root/'niro-steering.mesh').open('wb') as f:
  f.write(struct.pack('<I',len(parts)))
  for v,idx,mat in parts:
    f.write(struct.pack('<III',len(v),len(idx),mat));f.write(v.tobytes());f.write(idx.tobytes())
print('Triangles body / front left / front right:',totals,'parts',len(parts))
