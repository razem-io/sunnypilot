#!/usr/local/venv/bin/python3
"""Explicit one-time installation of matching C3/AGNOS 12.8 boot artwork.
Run with sudo and --install or --restore. Never runs from the normal launcher.
"""
import hashlib
import io
import os
from pathlib import Path
import struct
import subprocess
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parent
BACKUP = Path('/data/local/niro-boot/early-logo-backup')
PART = Path('/dev/disk/by-partlabel/splash')
BG = Path('/usr/comma/bg.jpg')
SERVICE = Path('/usr/lib/systemd/system/weston.service')
ORIGINAL = '5c61260048f22ede6e6343fabb27f6ff73f9271f4751a01aaf7abf097afc1f08'
OFFSET = 16384
PIXEL_OFFSET = OFFSET + 54
PIXEL_LENGTH = 1080 * 2160 * 3

def digest(data):
    return hashlib.sha256(data).hexdigest()

def write_partition(image):
    # Preserve partition metadata, BMP header and every byte after its pixels.
    with PART.open('r+b', buffering=0) as f:
        f.seek(PIXEL_OFFSET)
        f.write(image[PIXEL_OFFSET:PIXEL_OFFSET + PIXEL_LENGTH])
        os.fsync(f.fileno())
    if PART.read_bytes() != image:
        raise RuntimeError('Splash read-back mismatch')

def replace_file(path, data):
    tmp = path.with_name(path.name + '.niro-next')
    mode = path.stat().st_mode & 0o777
    with tmp.open('wb') as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    tmp.chmod(mode)
    tmp.replace(path)

def main():
    if sys.argv[1:] not in [['--install'], ['--restore']]:
        raise SystemExit('Usage: sudo /usr/local/venv/bin/python3 early-logo.py --install|--restore')
    assert os.geteuid() == 0, 'Root required'
    assert Path('/VERSION').read_text().strip() == '12.8', 'Only AGNOS 12.8 is validated'
    assert Path('/sys/firmware/devicetree/base/model').read_bytes().rstrip(b'\0') == b'comma tici'
    assert Path('/sys/devices/platform/vendor/vendor:gpio-som-id/som_id').read_text().strip() == '8'
    current = PART.read_bytes()
    if BACKUP.exists():
        original = (BACKUP/'splash.img').read_bytes()
    else:
        assert sys.argv[1] == '--install'
        assert digest(current) == ORIGINAL, 'Unrecognized splash; refusing to overwrite'
        BACKUP.mkdir(parents=True)
        (BACKUP/'splash.img').write_bytes(current)
        (BACKUP/'bg.jpg').write_bytes(BG.read_bytes())
        (BACKUP/'weston.service').write_bytes(SERVICE.read_bytes())
        os.sync()
        original = (BACKUP/'splash.img').read_bytes()
    assert digest(original) == ORIGINAL, 'Backup checksum mismatch'
    assert original[:8] == b'DDPH\x01\0\0\0'
    assert original[OFFSET:OFFSET+2] == b'BM'
    assert struct.unpack_from('<iiHHI',original,OFFSET+18) == (1080,2160,1,24,0)
    image = Image.open(ROOT/'early-logo-portrait.png').convert('RGB')
    assert image.size == (1080,2160)
    bmp = io.BytesIO()
    image.save(bmp,format='BMP')
    pixels = bmp.getvalue()[54:]
    assert len(pixels) == PIXEL_LENGTH
    updated = original[:PIXEL_OFFSET] + pixels + original[PIXEL_OFFSET+PIXEL_LENGTH:]
    assert current in (original, updated) or digest(current) == '7cd70e2dc50ded803a8e4b87dbc85ee5fe15388a769f13469a2bd5aaf1595d74', 'Unknown modified splash; refusing to overwrite'
    original_bg = (BACKUP/'bg.jpg').read_bytes()
    original_service = (BACKUP/'weston.service').read_bytes()
    old = b'.save("/tmp/bg.jpg")'
    new = b'.save("/tmp/bg.jpg", format="PNG")'
    # The unit uses shell-escaped double quotes.
    old = old.replace(b'"',b'\\"')
    new = new.replace(b'"',b'\\"')
    assert original_service.count(old) == 1, 'Unrecognized Weston setup'
    updated_service = original_service.replace(old,new)
    updated_bg = (ROOT/'early-logo-portrait.png').read_bytes()
    assert BG.read_bytes() in (original_bg, updated_bg) or digest(BG.read_bytes()) == 'df60bf3ee8791e327520e5d1f4aafa5410b8905fcd00c2852e2e45af8c9033ac', 'Background modified by another installation'
    assert SERVICE.read_bytes() in (original_service,updated_service), 'Weston unit modified by another installation'
    restoring = sys.argv[1] == '--restore'
    target, bg, service = (original,original_bg,original_service) if restoring else (updated,updated_bg,updated_service)
    previous_bg, previous_service = BG.read_bytes(), SERVICE.read_bytes()
    subprocess.run(['mount','-o','remount,rw','/'],check=True)
    try:
        try:
            replace_file(BG,bg)
            replace_file(SERVICE,service)
            write_partition(target)
            os.sync()
        except BaseException:
            replace_file(BG,previous_bg)
            replace_file(SERVICE,previous_service)
            write_partition(current)
            os.sync()
            raise
    finally:
        subprocess.run(['mount','-o','remount,ro','/'],check=True)
    subprocess.run(['systemctl','daemon-reload'],check=True)
    print(('Restored' if restoring else 'Installed') + ' boot artwork; read-back verified; no reboot triggered.')
    print('Splash SHA256:',digest(target))
    print('Original files preserved in',BACKUP)

if __name__ == '__main__':
    main()
