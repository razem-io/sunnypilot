#!/usr/bin/env python3
"""Install only the verified UI release carried by this branch, with no rollback."""
import hashlib, os, subprocess
from pathlib import Path
from manifest import ROOT, verify

def atomic_copy(source, target):
    target.parent.mkdir(parents=True,exist_ok=True)
    data=source.read_bytes()
    if target.is_file() and hashlib.sha256(target.read_bytes()).digest()==hashlib.sha256(data).digest():return False
    tmp=target.with_name(target.name+'.niro-next')
    with tmp.open('wb') as out:
        out.write(data);out.flush();os.fsync(out.fileno())
    tmp.chmod(0o755);os.replace(tmp,target)
    fd=os.open(target.parent,os.O_DIRECTORY)
    try:os.fsync(fd)
    finally:os.close(fd)
    return True

def main():
    manifest=verify() # Validate the complete package before touching the active UI.
    base=ROOT.parents[1]
    atomic_copy(ROOT/'release/niro-ui',base/'selfdrive/ui/ui')
    projection=ROOT/'projection'
    state=Path('/data/local/niro-projection');state.mkdir(parents=True,exist_ok=True)
    # TLS identity and runtime diagnostics survive branch updates; never commit keys.
    for link,target in [(projection/'.runtime',state),(projection/'qcom-encoder-probe',ROOT/'release/qcom-encoder')]:
        if link.is_symlink():link.unlink()
        elif link.exists():raise RuntimeError('Unexpected projection runtime path: '+str(link))
        link.symlink_to(target)
    (projection/'autoconnect.enabled').touch()
    subprocess.run(['bash',str(projection/'device/start-autoconnect.sh')],check=True)
    print('Branch dashboard ready:',manifest['files']['release/niro-ui'],flush=True)

if __name__=='__main__':main()
