#!/usr/bin/env python3
"""Build a native PS5 title wrapper using vendored, GPL-licensed native tooling."""
import hashlib,json,os,pathlib,re,shutil,subprocess,zipfile
ROOT=pathlib.Path(__file__).resolve().parent.parent
os.chdir(ROOT)
sdk=pathlib.Path(os.environ.get('PS5_PAYLOAD_SDK',ROOT/'.toolchain/opt/ps5-payload-sdk')).resolve()
llvm=os.environ.get('LLVM_CONFIG','llvm-config')
clang=os.environ.get('PS5_CLANG') or str(pathlib.Path(subprocess.check_output([llvm,'--bindir'],text=True).strip())/'clang')
native=ROOT/'third_party/ps5-native/tooling/native'
build=ROOT/'build/launcher';build.mkdir(parents=True,exist_ok=True)
def run(*args):subprocess.run([str(x) for x in args],check=True)
tool=ROOT/'build/ps5-native-tool';libc=ROOT/'build/libc-builder'
if not tool.is_file() or not libc.is_file():raise SystemExit('Run make native-tools first.')
for name in ('a','b'):
 run(libc,native/'runtime/api-surface.txt',native/'runtime/imports.txt',build/f'libc-{name}.elf')
 run(tool,'self','--sign','--in',build/f'libc-{name}.elf','--out',build/f'libc-{name}.prx')
expected='e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036'
for name in ('a','b'):
 if hashlib.sha256((build/f'libc-{name}.prx').read_bytes()).hexdigest()!=expected:raise SystemExit('Runtime reproduction check failed')
objects=[]
for source in (ROOT/'launcher/main.cpp',native/'app_crt.cpp'):
 obj=build/(source.stem+'.o');objects.append(obj)
 run(clang,'-target','x86_64-sie-ps5','-fvisibility-nodllstorageclass=default','-isysroot',sdk,'-isystem',sdk/'target/include/c++/v1','-isystem',sdk/'target/include','-fno-stack-protector','-fno-plt','-femulated-tls','-std=c++20','-O2','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-ffunction-sections','-fdata-sections','-c',source,'-o',obj)
run(sdk/'bin/prospero-lld','-T',native/'ps5-pie.ld','--eh-frame-hdr','--version-script',native/'app-symbols.map','-e','_start','-o',build/'launcher-pie.elf',*objects,'--as-needed',*sorted((sdk/'target/lib').glob('*.so')))
run(tool,'link','--in',build/'launcher-pie.elf','--out',build/'eboot.elf','--stub-dir',sdk/'target/lib','--module-sdk','0x02000009','--companion-sdk','0x08050001','--file-name','eboot.elf')
param=json.loads((ROOT/'launcher/sce_sys/param.json').read_text())
if param.get('titleId')!='PPSA99841' or param.get('conceptId')!='99841':raise SystemExit('Installer identity must match PPSA99841 / 99841')
if not re.fullmatch(r'[A-Z]{2}\d{4}-PPSA99841_00-[A-Z0-9]{16}',param.get('contentId','')):raise SystemExit('Invalid PS5 content ID')
if not re.fullmatch(r'\d{2}\.\d{3}\.\d{3}',param.get('contentVersion','')):raise SystemExit('Invalid PS5 content version')
app=ROOT/'dist'/param['titleId'];app.mkdir(parents=True,exist_ok=True)
(app/'sce_module').mkdir(exist_ok=True);(app/'sce_sys').mkdir(exist_ok=True)
run(tool,'self','--sign','--in',build/'eboot.elf','--out',app/'eboot.bin','--magic','0x1D3D154F')
shutil.copyfile(build/'libc-a.prx',app/'sce_module/libc.prx')
shutil.copyfile(ROOT/'dist/retro-attachments.elf',app/'retroatlas.elf')
for file in ('icon0.png','param.json'):shutil.copyfile(ROOT/'launcher/sce_sys'/file,app/'sce_sys'/file)
(app/'.retroatlas-owned').write_text('RetroAtlas:home-screen-launcher:v1\n')
for file in ('eboot.bin','sce_module/libc.prx','retroatlas.elf'):(app/file).chmod(0o755)
for file in (app/'eboot.bin',app/'sce_module/libc.prx'):run(tool,'self','--inspect','--file',file)
with zipfile.ZipFile(ROOT/'dist'/f'{param["titleId"]}.zip','w',zipfile.ZIP_DEFLATED) as z:
 for file in sorted(app.rglob('*')):
  if file.is_file():z.write(file,file.relative_to(app.parent))
print('Native app folder:',app)
