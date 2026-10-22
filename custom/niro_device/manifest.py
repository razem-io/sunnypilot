"""Release integrity: assets, source/build inputs, executables and projection code."""
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
EXCLUDED={'release/manifest.json','projection/.runtime','projection/qcom-encoder-probe','projection/autoconnect.enabled'}
def files():
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(ROOT.rglob('*')) if p.is_file() and not p.is_symlink()
            and not any(part in ('.build','__pycache__') for part in p.relative_to(ROOT).parts)
            and str(p.relative_to(ROOT)) not in EXCLUDED}
def verify():
    manifest=json.loads((ROOT/'release/manifest.json').read_text())
    for name,digest in manifest['files'].items():
        p=ROOT/name
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:
            raise RuntimeError('Release checksum mismatch: '+name)
    for name in ('niro-ui','qcom-encoder'):
        data=(ROOT/'release'/name).read_bytes()[:20]
        if data[:4]!=b'\x7fELF' or int.from_bytes(data[18:20],'little')!=183:
            raise RuntimeError('Not an AArch64 ELF: '+name)
    return manifest
if __name__=='__main__':
    if '--write' in sys.argv:
        (ROOT/'release/manifest.json').write_text(json.dumps({'format':1,'files':files()},indent=2)+'\n')
    else:verify();print('Release checksums and AArch64 binaries verified')
