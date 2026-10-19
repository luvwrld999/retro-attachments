#!/usr/bin/env python3
"""Exercise the real bootstrap against temporary storage and a local HTTP server."""
import http.server, os, pathlib, shlex, socket, subprocess, tempfile, threading, urllib.parse
root = pathlib.Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='retro-bridge-test-') as temp:
    work = pathlib.Path(temp)
    stage = work / 'stage with spaces'
    blob = b'\x7fELF\x02\x01' + bytes(range(256)) * 200
    (work / 'payload_blob.hpp').write_text('#include <cstddef>\ninline const unsigned char payload_blob[]={' + ','.join(map(str,blob)) + '};\ninline constexpr size_t payload_blob_size=sizeof(payload_blob);\ninline constexpr char payload_filename[]="test-viewer.elf";\n')
    requests = []
    class Handler(http.server.BaseHTTPRequestHandler):
        status = 200
        def do_GET(self):
            parsed = urllib.parse.urlparse(self.path)
            query = urllib.parse.parse_qs(parsed.query)
            requests.append((parsed.path, query))
            assert pathlib.Path(query['path'][0]).read_bytes() == blob
            self.send_response(self.status)
            self.end_headers()
        def log_message(self, *args): pass
    server = http.server.ThreadingHTTPServer(('127.0.0.1',0), Handler)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    flags = shlex.split(subprocess.check_output(['curl-config','--cflags','--libs'],text=True))
    binary = work / 'bridge'
    subprocess.run([os.environ.get('HOST_CXX','clang++'),'-std=c++17','-Wall','-Wextra','-Werror','-Isrc','-I'+str(work),'-DRETRO_STAGE_DIR="'+str(stage)+'"','-DRETRO_WEBSRV_URL="http://127.0.0.1:'+str(server.server_port)+'"','launcher/payload_bootstrap.cpp','-o',str(binary),*flags],cwd=root,check=True)
    def run(expected): assert subprocess.run([str(binary)],timeout=50).returncode == expected
    run(0)
    assert requests == [('/hbldr',{'daemon':['0'],'pipe':['0'],'path':[str(stage/'test-viewer.elf')]})]
    assert (stage/'test-viewer.elf').stat().st_mode & 0o777 == 0o755
    assert not list(stage.glob('.stage-*'))
    # A corrupted old copy is fully replaced before requesting launch.
    (stage/'test-viewer.elf').write_bytes(b'old')
    run(0)
    Handler.status = 503
    run(1)
    assert len(requests) == 3
    server.shutdown(); server.server_close()
    run(1) # Connection refused, no false success.
    (stage/'test-viewer.elf').unlink(); stage.rmdir()
    other = work/'other'; other.mkdir(); stage.symlink_to(other,target_is_directory=True)
    run(1)
    assert not list(other.iterdir())
    stage.unlink(); stage.write_text('not a directory')
    run(1)
print('PASS: bridge byte verification, repeat launch, URL encoding, graphical flags, permissions, HTTP rejection, offline service and invalid staging paths')
