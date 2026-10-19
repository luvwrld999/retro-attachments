#!/usr/bin/env python3
import pathlib,subprocess,sys
if sys.platform=='darwin':
 source='.section __TEXT,__const\n.balign 16\n.global _installer_blob\n_installer_blob:\n.incbin "build/installer_bundle.bin"\n'
else:
 source=pathlib.Path('build/installer_bundle.S').read_text()
pathlib.Path('build/installer_bundle_host.S').write_text(source)
subprocess.run(['clang','-c','build/installer_bundle_host.S','-o','build/installer_bundle_host.o'],check=True)
