"""Install the native Qt card extension into this exact release UI, reversibly."""
import hashlib,os,pathlib,shutil,subprocess
from cereal import messaging
root=pathlib.Path('/data/openpilot');state=pathlib.Path('/data/local/niro-dashboard');ui=root/'selfdrive/ui/ui'
expected='424c259c88f275c71e34c34f35aed7e38b535d7bfbcf12d5057a69640a84b675'
sm=messaging.SubMaster(['deviceState','selfdriveState','carControl'])
for _ in range(15):sm.update(100)
assert sm.alive['deviceState'] and sm.valid['deviceState'] and not sm['deviceState'].started,'Only install while confirmed offroad'
assert not sm['selfdriveState'].enabled and not sm['carControl'].latActive and not sm['carControl'].longActive,'Controls must be inactive'
assert hashlib.sha256(ui.read_bytes()).hexdigest()==expected,'Unexpected UI build: refusing binary change'
backup=state/'ui.before-native-entry'
if backup.exists():assert hashlib.sha256(backup.read_bytes()).hexdigest()==expected
else:shutil.copy2(ui,backup)
staged=ui.with_name('ui.niro-next');shutil.copy2(ui,staged)
patch=state/'tools/bin/patchelf';lib=root/'custom/niro_dashboard/libniro-entry.so'
subprocess.run([str(patch),'--add-needed',str(lib),str(staged)],check=True)
needed=subprocess.check_output([str(patch),'--print-needed',str(staged)],text=True)
assert str(lib) in needed
os.replace(staged,ui)
print('Installed native card extension. Original UI backed up at',backup)
print('Patched UI SHA256',hashlib.sha256(ui.read_bytes()).hexdigest())
