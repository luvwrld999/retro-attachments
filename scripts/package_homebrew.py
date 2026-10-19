#!/usr/bin/env python3
"""Package graphical apps in websrv's documented eboot.elf directory layout."""
import hashlib, json, pathlib, zipfile
root = pathlib.Path(__file__).resolve().parent.parent
dist = root / 'dist'
entries = {}
for folder, binary in [('RetroAttachments', 'retro-attachments.elf'),
                       ('RetroAttachmentsInstaller', 'retro-attachments-installer.elf')]:
    data = (dist / binary).read_bytes()
    assert data[:6] == b'\x7fELF\x02\x01'
    entries[f'homebrew/{folder}/eboot.elf'] = (data, 0o755)
    entries[f'homebrew/{folder}/sce_sys/icon0.png'] = ((root / 'launcher/sce_sys/icon0.png').read_bytes(), 0o644)
entries['INSTALL.md'] = ((root / 'docs/INSTALL.md').read_bytes(), 0o644)
entries['LICENSE'] = ((root / 'LICENSE').read_bytes(), 0o644)
entries['THIRD_PARTY.md'] = ((root / 'THIRD_PARTY.md').read_bytes(), 0o644)
manifest = {name: hashlib.sha256(data).hexdigest() for name, (data, mode) in entries.items()}
entries['manifest.json'] = (json.dumps(manifest, indent=2).encode(), 0o644)
archive = dist / 'Retro-Attachments-Homebrew-Launcher.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, (data, mode) in sorted(entries.items()):
        info = zipfile.ZipInfo(name)
        info.create_system = 3
        info.external_attr = (0o100000 | mode) << 16
        info.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(info, data)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert set(z.namelist()) == set(entries)
    for name, (data, mode) in entries.items():
        assert z.read(name) == data
        assert z.getinfo(name).external_attr >> 16 & 0o777 == mode
print('PASS: Homebrew Launcher app/installer paths, bytes, permissions and manifest')
