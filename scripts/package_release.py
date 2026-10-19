#!/usr/bin/env python3
"""Create explicit-allowlist source and hardware-test archives; never include user data."""
import hashlib,json,pathlib,zipfile,subprocess,sys
root=pathlib.Path(__file__).resolve().parent.parent
dist=root/'dist';dist.mkdir(exist_ok=True)
subprocess.run([sys.executable,str(root/'scripts/package_homebrew.py')],check=True)
files=[root/p for p in ('README.md','LICENSE','THIRD_PARTY.md','DEPENDENCY_SHA256.txt','CONTRIBUTING.md','Makefile','.gitignore')]
for folder in ('src','scripts','tests','third_party','assets','launcher','installer','docs','.github'):
 files.extend(p for p in (root/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc')
with zipfile.ZipFile(dist/'Retro-Attachments-source.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(files):z.write(p,pathlib.Path('Retro-Attachments-source')/p.relative_to(root))
with zipfile.ZipFile(dist/'Retro-Attachments-installer-test.zip','w',zipfile.ZIP_DEFLATED) as z:
 for name in ('RetroAttachments.elf','RetroAttachmentsInstaller.elf','Retro-Attachments-Payload-Manager.zip','retro-attachments.elf','retro-attachments-installer.elf','PPSA99841.zip','Retro-Attachments-Homebrew-Launcher.zip','installer-manifest.json','Retro-Attachments-source.zip'):z.write(dist/name,name)
 for name in ('README.md','LICENSE','THIRD_PARTY.md','docs/INSTALL.md','docs/PAYLOAD-MANAGER.md'):z.write(root/name,name)
 for name in ('installer-preview.png','themes.png','preview.png','detail-preview.png'):
  if (dist/name).exists():z.write(dist/name,name)
artifacts=['RetroAttachments.elf','RetroAttachmentsInstaller.elf','Retro-Attachments-Payload-Manager.zip','retro-attachments.elf','retro-attachments-installer.elf','PPSA99841.zip','Retro-Attachments-Homebrew-Launcher.zip','installer-manifest.json','Retro-Attachments-source.zip','Retro-Attachments-installer-test.zip']
(dist/'SHA256SUMS').write_text(''.join(hashlib.sha256((dist/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in artifacts))
print('Packaged',len(files),'source files, complete installer bundle and SHA256SUMS.')
