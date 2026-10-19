#!/usr/bin/env python3
"""Generate the original Retro Attachments pixel-style launcher icon, without external assets."""
import pathlib,struct,zlib
W=512
pix=bytearray(W*W*3)
def rect(x,y,w,h,c):
 for yy in range(max(0,y),min(W,y+h)):
  for xx in range(max(0,x),min(W,x+w)):
   off=(yy*W+xx)*3;pix[off:off+3]=bytes(c)
for y in range(W):
 for x in range(W):
  glow=max(0,1-((x-256)**2+(y-200)**2)**.5/370)
  off=(y*W+x)*3;pix[off:off+3]=bytes((int(9+glow*10),int(15+glow*21),int(30+glow*33)))
for y in range(60,391):
 for x in range(70,443):
  d=((x-256)**2+(y-225)**2)**.5
  if 174<d<177:rect(x,y,1,1,(51,90,119))
cyan=(100,207,255)
glyph={'R':['11110','10001','10001','11110','10100','10010','10001'],'A':['01110','10001','10001','11111','10001','10001','10001'],'T':['11111','00100','00100','00100','00100','00100','00100'],'L':['10000','10000','10000','10000','10000','10000','11111'],'S':['01111','10000','10000','01110','00001','00001','11110'],'E':['11111','10000','10000','11110','10000','10000','11111'],'O':['01110','10001','10001','10001','10001','10001','01110']}
glyph.update({'C':['01111','10000','10000','10000','10000','10000','01111'],'H':['10001','10001','10001','11111','10001','10001','10001'],'M':['10001','11011','10101','10101','10001','10001','10001'],'N':['10001','11001','11001','10101','10011','10011','10001']})
def text(s,x,y,size,c):
 for char in s:
  for row,bits in enumerate(glyph[char]):
   for col,b in enumerate(bits):
    if b=='1':rect(x+col*size,y+row*size,size,size,c)
  x+=6*size
text('RA',124,133,24,cyan)
rect(124,324,264,5,cyan)
text('RETRO',169,375,6,(235,246,255));text('ATTACHMENTS',126,433,4,(151,169,193))
def chunk(t,data):return struct.pack('>I',len(data))+t+data+struct.pack('>I',zlib.crc32(t+data)&0xffffffff)
raw=b''.join(b'\0'+pix[y*W*3:(y+1)*W*3] for y in range(W))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',W,W,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
pathlib.Path('launcher/sce_sys/icon0.png').write_bytes(png)
