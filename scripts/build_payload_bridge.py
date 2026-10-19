#!/usr/bin/env python3
"""Embed each graphical ELF in a non-SDL Payload Manager entry point."""
import hashlib, os, pathlib, subprocess, zipfile
root = pathlib.Path(__file__).resolve().parent.parent
os.chdir(root)
sdk = pathlib.Path(os.environ.get('PS5_PAYLOAD_SDK', root / '.toolchain/opt/ps5-payload-sdk'))
for name, source in [('RetroAttachments', 'retro-attachments.elf'), ('RetroAttachmentsInstaller', 'retro-attachments-installer.elf')]:
    data = (root / 'dist' / source).read_bytes()
    assert data[:6] == b'\x7fELF\x02\x01'
    build = root / 'build' / ('bridge-' + name)
    build.mkdir(parents=True, exist_ok=True)
    (build / 'payload.bin').write_bytes(data)
    filename = name + '-' + hashlib.sha256(data).hexdigest() + '.elf'
    (build / 'payload_blob.hpp').write_text('#pragma once\n#include <cstddef>\nextern "C" const unsigned char payload_blob[];\ninline constexpr size_t payload_blob_size=' + str(len(data)) + ';\ninline constexpr char payload_filename[]="' + filename + '";\n')
    (build / 'payload.S').write_text('.section .rodata\n.balign 16\n.global payload_blob\npayload_blob:\n.incbin "' + str(build / 'payload.bin') + '"\n')
    subprocess.run([str(sdk / 'bin/prospero-clang'), '-c', str(build / 'payload.S'), '-o', str(build / 'payload.o')], check=True)
    subprocess.run([str(sdk / 'bin/prospero-clang++'), '-std=c++17', '-O2', '-Wall', '-Wextra', '-D__PROSPERO__', '-DRETRO_STARTUP_DIAGNOSTICS', '-Isrc', '-I' + str(build), '-I' + str(sdk / 'target/user/homebrew/include'), 'launcher/payload_bootstrap.cpp', str(build / 'payload.o'), '-o', 'dist/' + name + '.elf', '-lcurl', '-lpsl', '-lzstd', '-lssl', '-lcrypto', '-lz', '-lpthread'], check=True)
with zipfile.ZipFile(root / 'dist/Retro-Attachments-Payload-Manager.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for name in ['RetroAttachments', 'RetroAttachmentsInstaller']:
        z.write(root / 'dist' / (name + '.elf'), name + '/' + name + '.elf')
    z.write(root / 'docs/PAYLOAD-MANAGER.md', 'START-HERE.md')
print('Built self-contained Payload Manager viewer and installer bridges (no SDL linkage).')
