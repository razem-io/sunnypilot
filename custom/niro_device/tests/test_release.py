import importlib.util, json, sys, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import manifest
from install import atomic_copy

class ReleaseTests(unittest.TestCase):
    def test_modified_payload_is_rejected_before_install(self):
        original=manifest.ROOT
        with tempfile.TemporaryDirectory() as tmp:
            manifest.ROOT=Path(tmp)
            try:
                (manifest.ROOT/'release').mkdir()
                elf=b'\x7fELF'+b'\0'*14+(183).to_bytes(2,'little')
                for name in ('niro-ui','qcom-encoder'):(manifest.ROOT/'release'/name).write_bytes(elf)
                m={'files':manifest.files()}
                (manifest.ROOT/'release/manifest.json').write_text(json.dumps(m))
                manifest.verify()
                (manifest.ROOT/'release/niro-ui').write_bytes(b'corrupt')
                with self.assertRaisesRegex(RuntimeError,'checksum mismatch'):manifest.verify()
            finally:manifest.ROOT=original

    def test_next_release_replaces_ui_and_repeat_boot_is_noop(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'release';target=root/'ui/ui'
            source.write_bytes(b'first-release')
            self.assertTrue(atomic_copy(source,target))
            inode=target.stat().st_ino
            self.assertFalse(atomic_copy(source,target))
            self.assertEqual(inode,target.stat().st_ino)
            source.write_bytes(b'next-branch-release')
            self.assertTrue(atomic_copy(source,target))
            self.assertEqual(target.read_bytes(),b'next-branch-release')
            self.assertFalse(target.with_name('ui.niro-next').exists())

if __name__=='__main__':unittest.main()
