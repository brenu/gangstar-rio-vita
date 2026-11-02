#!/usr/bin/env python3
"""Generate an optional private lossless cache from the supported original OBB."""
import argparse,ctypes,ctypes.util,json,os,re,struct,zlib
from pathlib import Path
from stage_data import sha,INPUTS

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('obb',type=Path)
 p.add_argument('--output',type=Path,default=Path('work/lightmaps.lz4'))
 a=p.parse_args()
 if a.obb.stat().st_size!=2119380688:raise SystemExit('Unsupported main OBB size')
 expected=next(value for name,value in INPUTS.items() if '/main.12030.' in name)
 if sha(a.obb)!=expected:raise SystemExit('Original OBB revision/hash mismatch')
 library=ctypes.util.find_library('lz4')
 if not library:raise SystemExit('Install the host liblz4 library to prepare this optional cache')
 lib=ctypes.CDLL(library)
 lib.LZ4_compressBound.argtypes=[ctypes.c_int];lib.LZ4_compressBound.restype=ctypes.c_int
 lib.LZ4_compress_HC.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int];lib.LZ4_compress_HC.restype=ctypes.c_int
 lib.LZ4_decompress_safe.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int];lib.LZ4_decompress_safe.restype=ctypes.c_int
 manifest=Path(__file__).resolve().parents[1]/'source/gangstar_district_manifest.h'
 entries=[(name,int(offset),int(size)) for name,offset,size in re.findall(r'\{"([^"\n]*_lightmap[^"\n]*)",(\d+)u,(\d+)u\}',manifest.read_text())]
 assert 0<len(entries)<=128
 a.output.parent.mkdir(parents=True,exist_ok=True);temp=a.output.with_name(a.output.name+'.tmp')
 records=[];report=[]
 try:
  with a.obb.open('rb') as source,temp.open('w+b') as target:
   target.write(b'\0'*(16+20*len(entries)))
   for name,offset,size in entries:
    source.seek(offset);raw=source.read(size)
    if len(raw)!=size or raw[:4]!=b'DDS ':raise ValueError('Invalid original lightmap: '+name)
    packed=ctypes.create_string_buffer(lib.LZ4_compressBound(size))
    n=lib.LZ4_compress_HC(raw,packed,size,len(packed),12)
    if n<=0 or n>=size:raise ValueError('Lightmap did not compress: '+name)
    check=ctypes.create_string_buffer(size)
    if lib.LZ4_decompress_safe(packed,check,n,size)!=size or check.raw!=raw:raise ValueError('Round-trip verification failed: '+name)
    records.append((offset,size,target.tell(),n,zlib.crc32(raw)))
    target.write(packed.raw[:n]);report.append({'name':name,'original_bytes':size,'packed_bytes':n})
   target.seek(0);target.write(b'RLC1'+struct.pack('<III',2119380688,len(records),20))
   for record in records:target.write(struct.pack('<IIIII',*record))
   target.flush();os.fsync(target.fileno())
  os.replace(temp,a.output)
 except BaseException:
  temp.unlink(missing_ok=True);raise
 a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Private cache:',a.output,'entries:',len(records),'bytes:',a.output.stat().st_size,'all decompressed outputs verified')
if __name__=='__main__':main()
