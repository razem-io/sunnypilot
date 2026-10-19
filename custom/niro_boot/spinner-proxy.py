"""Hide only the competing startup spinner while our splash owns startup.
The caller's lifetime and stdin protocol remain intact. Build-error windows
are separate commands and are not intercepted. Failures use original spinner.
"""
import os
import select
import struct
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
STATE = Path(os.environ.get('NIRO_BOOT_STATE_DIR', '/data/local/niro-boot'))
def splash_alive():
    try:
        pid = int(Path('/dev/shm/niro-boot.lock').read_text().splitlines()[0])
        return Path(f'/proc/{pid}/exe').resolve() == ROOT/'niro-splash'
    except (OSError, ValueError, IndexError):
        return False

def ui_ready():
    for p in Path('/proc').iterdir():
        if not p.name.isdigit():
            continue
        try:
            if (p/'comm').read_text().strip() != 'ui':
                continue
            if not str((p/'exe').resolve()).endswith('/selfdrive/ui/ui'):
                continue
            ns, = struct.unpack('<Q',Path('/dev/shm/wd_'+p.name).read_bytes())
            age = time.clock_gettime(time.CLOCK_BOOTTIME) - ns/1e9
            if 0 <= age < 3:
                return True
        except (OSError,ValueError,struct.error):
            continue
    return False

def main():
    logged = False
    while True:
        if not splash_alive() and not ui_ready():
            with (STATE/'boot.log').open('a') as f:
                f.write('SPINNER_FALLBACK: splash unavailable, original spinner restored\n')
            os.execv('/usr/local/venv/bin/python3',['python3','./spinner.py'])
        if not logged:
            with (STATE/'boot.log').open('a') as f:
                f.write('SPINNER_SUPPRESSED: Kia splash or ready UI owns display\n')
            logged = True
        if select.select([sys.stdin],[],[],.1)[0]:
            if not sys.stdin.buffer.readline():
                return

if __name__ == '__main__':
    try:
        main()
    except Exception:
        os.execv('/usr/local/venv/bin/python3',['python3','./spinner.py'])
