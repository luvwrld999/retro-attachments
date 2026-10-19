#!/usr/bin/env python3
import hashlib,json,pathlib
root=pathlib.Path(__file__).resolve().parent.parent
param=json.loads((root/'launcher/sce_sys/param.json').read_text());app=root/'dist'/param['titleId'];build=root/'build'
paths=['eboot.bin','sce_module/libc.prx','retroatlas.elf','sce_sys/icon0.png','sce_sys/param.json','.retroatlas-owned']
blob=bytearray();rows=[];manifest=[]
for path in paths:
 b=(app/path).read_bytes();digest=hashlib.sha256(b).hexdigest();mode=0o755 if path.endswith(('.bin','.prx','.elf')) else 0o644
 rows.append('{'+json.dumps(path)+f', {len(blob)}, {len(b)}, '+json.dumps(digest)+f', {oct(mode).replace("0o","0")}'+'}')
 manifest.append(dict(path=path,size=len(b),sha256=digest,mode=oct(mode)));blob+=b
(build/'installer_bundle.bin').write_bytes(blob)
(build/'installer_bundle.hpp').write_text('#pragma once\n#include "install.hpp"\nextern "C" const unsigned char installer_blob[];\ninline constexpr std::size_t installer_blob_size='+str(len(blob))+';\ninline const install::Entry installer_files[]={\n'+',\n'.join(rows)+'\n};\n')
(build/'installer_bundle.S').write_text('.section .rodata\n.balign 16\n.global installer_blob\ninstaller_blob:\n.incbin "build/installer_bundle.bin"\n')
(root/'dist/installer-manifest.json').write_text(json.dumps(dict(title_id=param['titleId'],version=param['contentVersion'],files=manifest),indent=2)+'\n')
