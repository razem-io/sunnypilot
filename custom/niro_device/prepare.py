#!/usr/bin/env python3
"""Materialize the locked legacy Qt dependency, then apply the device-only patch."""
import argparse, json, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parent

def run(*args, **kwargs): subprocess.run(args, check=True, **kwargs)
def prepare(cache=None):
    versions=json.loads((ROOT/'versions.json').read_text())
    out=ROOT/'.build';out.mkdir(exist_ok=True)
    source=out/'source'
    if source.exists():raise SystemExit('Build source already exists; remove .build/source explicitly to regenerate it')
    run('git','clone','--no-checkout',cache or 'https://github.com/sunnypilot/sunnypilot.git',str(source),env={**os.environ,'GIT_LFS_SKIP_SMUDGE':'1'})
    run('git','-C',str(source),'checkout','--detach',versions['qt_source'],env={**os.environ,'GIT_LFS_SKIP_SMUDGE':'1'})
    # Fetch only the Qt resource assets, not neural-network weights or routes.
    if cache:
        shutil.copytree(Path(cache)/'selfdrive/assets',source/'selfdrive/assets',dirs_exist_ok=True)
    else:run('git','-C',str(source),'lfs','pull','--include=selfdrive/assets/**')
    msg=source/'msgq_repo'
    if cache:
        run('git','clone','--no-checkout',str(Path(cache)/'msgq_repo'),str(msg))
        run('git','-C',str(msg),'checkout','--detach',versions['msgq'])
    else:
        run('git','-C',str(source),'submodule','update','--init','msgq_repo')
        assert subprocess.check_output(['git','-C',str(msg),'rev-parse','HEAD'],text=True).strip()==versions['msgq']
    (source/'cereal/car.capnp').unlink()
    shutil.copy2(ROOT/'patches/car.capnp',source/'cereal/car.capnp')
    run('git','-C',str(source),'apply',str(ROOT/'patches/device.patch'))
    run('git','-C',str(msg),'apply',str(ROOT/'patches/msgq.patch'))
    shutil.copy2(ROOT/'native/main.cc',source/'selfdrive/ui/main.cc')
    print(source)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source-cache',help='optional existing checkout to avoid downloading immutable dependencies')
    prepare(p.parse_args().source_cache)
