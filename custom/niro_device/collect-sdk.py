import pathlib,subprocess,tarfile,sys
paths=set()
qt=['Core','Gui','Widgets','Network','Concurrent','DBus','Xml','Svg']
libs=[pathlib.Path('/usr/lib/aarch64-linux-gnu')/f'libQt5{x}.so' for x in qt]
libs += list(pathlib.Path('/usr/local/lib').glob('libcapnp*.so*'))+list(pathlib.Path('/usr/local/lib').glob('libkj*.so*'))
libs += [pathlib.Path('/usr/lib/aarch64-linux-gnu')/x for x in ['libGLESv2_adreno.so','libEGL_adreno.so','libOpenCL.so']]
tools=[pathlib.Path('/usr/lib/qt5/bin/moc'),pathlib.Path('/usr/lib/qt5/bin/rcc'),pathlib.Path('/usr/local/bin/capnp'),pathlib.Path('/usr/local/bin/capnpc-c++')]
for p in libs+tools:
 if not p.exists():continue
 paths.add(p)
 result=subprocess.run(['ldd',str(p)],capture_output=True,text=True)
 for line in result.stdout.splitlines():
  parts=line.split()
  if '=>' in parts and len(parts)>2 and parts[2].startswith('/'):paths.add(pathlib.Path(parts[2]))
for p in ['/usr/include/aarch64-linux-gnu/qt5','/usr/local/include/capnp','/usr/local/include/kj']:
 if pathlib.Path(p).exists():paths.add(pathlib.Path(p))
with tarfile.open(fileobj=sys.stdout.buffer,mode='w|gz',dereference=True) as tar:
 for p in sorted(paths):tar.add(p,arcname=str(p).lstrip('/'))
