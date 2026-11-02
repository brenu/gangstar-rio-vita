#!/usr/bin/env python3
"""Create private, byte-exact benchmark inputs from the user's original OBB."""
import argparse,json,re,struct,zlib,hashlib,ctypes,ctypes.util
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('obb',type=Path);p.add_argument('--output',type=Path,default=Path('work/lightmap-benchmark.bin'))
p.add_argument('--codec',choices=['zlib','lz4'],default='zlib')
a=p.parse_args()
if a.obb.stat().st_size!=2119380688:raise SystemExit('Unexpected original main OBB size')
manifest=Path(__file__).resolve().parents[1]/'source/gangstar_district_manifest.h'
entries={name:(int(offset),int(size)) for name,offset,size in re.findall(r'\{"([^"\n]+)",(\d+)u,(\d+)u\}',manifest.read_text())}
lz4=None
if a.codec=='lz4':
 library=ctypes.util.find_library('lz4')
 if not library:raise SystemExit('LZ4 input preparation requires the host liblz4 library')
 lz4=ctypes.CDLL(library)
 lz4.LZ4_compressBound.argtypes=[ctypes.c_int];lz4.LZ4_compressBound.restype=ctypes.c_int
 lz4.LZ4_compress_HC.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int];lz4.LZ4_compress_HC.restype=ctypes.c_int
 lz4.LZ4_decompress_safe.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int];lz4.LZ4_decompress_safe.restype=ctypes.c_int
result=bytearray((b'RLB2' if lz4 else b'RLB1')+struct.pack('<I',2));report=[]
with a.obb.open('rb') as source:
 for name in ['copacabana_lightmap00.tga','southfavela_lightmap00.tga']:
  offset,size=entries[name];source.seek(offset);raw=source.read(size)
  if len(raw)!=size or raw[:4]!=b'DDS ':raise SystemExit('Invalid original DDS: '+name)
  if lz4:
   target=ctypes.create_string_buffer(lz4.LZ4_compressBound(size))
   n=lz4.LZ4_compress_HC(raw,target,size,len(target),12)
   if n<=0:raise SystemExit('LZ4 compression failed')
   packed=target.raw[:n];check=ctypes.create_string_buffer(size)
   assert lz4.LZ4_decompress_safe(packed,check,n,size)==size and check.raw==raw
  else:
   packed=zlib.compress(raw,1);assert zlib.decompress(packed)==raw
  result+=struct.pack('<IIII',offset,size,len(packed),zlib.crc32(raw))+packed
  report.append({'name':name,'source_offset':offset,'original_bytes':size,'compressed_bytes':len(packed),'sha256':hashlib.sha256(raw).hexdigest()})
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(result)
a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
print('Private benchmark input:',a.output,len(result),'bytes; both entries verified byte-for-byte')
