"""Convert the supplied GLB to simple interleaved meshes for the device renderer."""
import json,struct,pathlib,sys
import numpy as np
src=pathlib.Path(sys.argv[1]); out=pathlib.Path(__file__).parent/'assets'
b=src.read_bytes(); n=struct.unpack_from('<I',b,12)[0]; j=json.loads(b[20:20+n]); data=b[28+n:]
def accessor(i):
 a=j['accessors'][i];v=j['bufferViews'][a['bufferView']];dtype={5126:'<f4',5125:'<u4',5123:'<u2'}[a['componentType']];k={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];offset=v.get('byteOffset',0)+a.get('byteOffset',0)
 return np.ndarray((a['count'],k),dtype=dtype,buffer=data,offset=offset,strides=(v.get('byteStride',np.dtype(dtype).itemsize*k),np.dtype(dtype).itemsize)).copy()
meshes=[]
def visit(i,parent):
 node=j['nodes'][i];m=parent@np.array(node.get('matrix',np.eye(4).flatten(order='F')),dtype=float).reshape(4,4,order='F')
 if 'mesh' in node:
  for p in j['meshes'][node['mesh']]['primitives']:
   a=p['attributes'];pos=accessor(a['POSITION']);normal=accessor(a['NORMAL']);uv=accessor(a['TEXCOORD_0']);pos=(np.c_[pos,np.ones(len(pos))]@m.T)[:,:3];normal=normal@np.linalg.inv(m[:3,:3]);meshes.append([pos,normal,uv,accessor(p['indices']).flatten(),p['material']])
 for c in node.get('children',[]):visit(c,m)
for i in j['scenes'][j.get('scene',0)]['nodes']:visit(i,np.eye(4))
positions=np.concatenate([x[0] for x in meshes]);size=np.ptp(positions,axis=0);angle=3*np.pi/2 if size[2]>size[0] else np.pi;c,s=np.cos(angle),np.sin(angle);rot=np.array([[c,0,s],[0,1,0],[-s,0,c]])
positions=positions@rot.T;lo=positions.min(0);hi=positions.max(0);center=(lo+hi)/2;center[1]=lo[1];scale=4.4/(hi[0]-lo[0])
with (out/'niro.mesh').open('wb') as f:
 f.write(struct.pack('<I',len(meshes)))
 for pos,normal,uv,idx,material in meshes:
  pos=(pos@rot.T-center)*scale;normal=normal@rot.T;normal/=np.linalg.norm(normal,axis=1)[:,None];verts=np.c_[pos,normal,uv].astype('<f4');f.write(struct.pack('<III',len(pos),len(idx),material));f.write(verts.tobytes());f.write(idx.astype('<u4').tobytes())
for mi,material in enumerate(j['materials']):
 tex=material.get('pbrMetallicRoughness',{}).get('baseColorTexture')
 if tex:
  im=j['images'][j['textures'][tex['index']]['source']];v=j['bufferViews'][im['bufferView']];start=v.get('byteOffset',0);(out/f'{mi}.png').write_bytes(data[start:start+v['byteLength']])
print('Converted',len(meshes),'meshes; dimensions',((hi-lo)*scale).tolist())
