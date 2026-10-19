#!/usr/bin/env python3
"""Offline structural checks; deliberately makes no firmware-compatibility claim."""
import hashlib,json,pathlib,re,struct,zipfile
root=pathlib.Path(__file__).resolve().parent.parent
app=root/'dist/PPSA99841'
p=json.loads((app/'sce_sys/param.json').read_text())
assert p['titleId']=='PPSA99841' and p['conceptId']=='99841'
assert re.fullmatch(r'[A-Z]{2}\d{4}-PPSA99841_00-[A-Z0-9]{16}',p['contentId'])
assert re.fullmatch(r'\d{2}\.\d{3}\.\d{3}',p['contentVersion'])
assert p['localizedParameters']['en-US']['titleName']=='Retro Attachments'
assert p['applicationCategoryType']==0 and p['contentBadgeType']==1
assert any(i['intentType']=='launchActivity' for i in p['gameIntent']['permittedIntents'])
assert (app/'.retroatlas-owned').read_text()=='RetroAtlas:home-screen-launcher:v1\n'
for name in ('eboot.bin','sce_module/libc.prx'):
 assert struct.unpack('<I',(app/name).read_bytes()[:4])[0] == (0x1d3d154f if name == 'eboot.bin' else 0xeef51454)
 assert (app/name).stat().st_mode&0o111
png=(app/'sce_sys/icon0.png').read_bytes()
assert png[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',png[16:24])==(512,512)
for name in ('retro-attachments.elf','retro-attachments-installer.elf'):
 data=(root/'dist'/name).read_bytes();assert data[:6]==b'\x7fELF\x02\x01' and struct.unpack('<H',data[18:20])[0]==62
manifest=json.loads((root/'dist/installer-manifest.json').read_text())
assert manifest['title_id']==p['titleId'] and manifest['version']==p['contentVersion']
for entry in manifest['files']:
 data=(app/entry['path']).read_bytes();assert len(data)==entry['size'] and hashlib.sha256(data).hexdigest()==entry['sha256']
with zipfile.ZipFile(root/'dist/PPSA99841.zip') as z:
 assert z.testzip() is None
 assert set(z.namelist())=={'PPSA99841/'+f['path'] for f in manifest['files']}
 for f in manifest['files']:assert z.read('PPSA99841/'+f['path'])==(app/f['path']).read_bytes()
print('PASS: native title metadata, icon, executable modes, ELF architecture, app archive and installer manifest')
