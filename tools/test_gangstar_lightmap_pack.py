"""Exercise the actual optional pack reader with synthetic, non-game payloads."""
from pathlib import Path
import subprocess,tempfile,struct,zlib,ctypes
with tempfile.TemporaryDirectory(prefix='rio-pack-') as temp:
 d=Path(temp);shared=d/'lz4.so'
 subprocess.run(['cc','-O2','-shared','-fPIC','lib/lz4/lz4.c','-o',str(shared)],check=True)
 lib=ctypes.CDLL(str(shared));lib.LZ4_compressBound.argtypes=[ctypes.c_int]
 lib.LZ4_compress_default.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int]
 size=2796344;raw=(bytes(range(256))*11000)[:size];out=ctypes.create_string_buffer(lib.LZ4_compressBound(size))
 n=lib.LZ4_compress_default(raw,out,size,len(out));assert 0<n<size
 blob=b'RLC1'+struct.pack('<III',2119380688,1,20)+struct.pack('<IIIII',183416410,size,36,n,zlib.crc32(raw))+out.raw[:n]
 pack=d/'lightmaps.lz4';pack.write_bytes(blob)
 harness=d/'main.c';harness.write_text(r'''
#include "gangstar_lightmap_pack.h"
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
static int fail;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t n){if(fail){fail=0;return NULL;}return __real_malloc(n);}
uint64_t sceKernelGetProcessTimeWide(void){return 1;}
void _log_print(int level,const char *fmt,...){(void)level;(void)fmt;}
static void *reader(void *argument){
 (void)argument;unsigned char *p=malloc(2796344);assert(p);
 for(unsigned k=0;k<3;++k){assert(gangstar_lightmap_pack_read(183416410,2796344,p));for(unsigned i=0;i<2796344;++i)assert(p[i]==(i&255));}
 free(p);return NULL;
}
int main(int argc,char **argv){
 (void)argv;unsigned char *p=malloc(2796344);assert(p);errno=123;
 if(argc>1){assert(!gangstar_lightmap_pack_read(183416410,2796344,p));assert(errno==123);assert(!gangstar_lightmap_pack_read(183416410,2796344,p));}
 else {
  fail=1;assert(!gangstar_lightmap_pack_read(183416410,2796344,p));assert(errno==123);
  assert(gangstar_lightmap_pack_read(183416410,2796344,p));assert(errno==123);
  for(unsigned i=0;i<2796344;++i)assert(p[i]==(i&255));
  assert(!gangstar_lightmap_pack_read(183416410,699192,p));
  assert(!gangstar_lightmap_pack_read(1,2796344,p));
  pthread_t threads[2];for(unsigned i=0;i<2;++i)assert(!pthread_create(&threads[i],NULL,reader,NULL));
  for(unsigned i=0;i<2;++i)assert(!pthread_join(threads[i],NULL));
 }
 free(p);return 0;
}
''')
 exe=d/'test'
 subprocess.run(['cc','-std=c11','-D_XOPEN_SOURCE=700','-DGANGSTAR_LIGHTMAP_PACK','-DDEBUG_SOLOADER',f'-DDATA_PATH="{d}/"','-O2','-Wall','-Wextra','-Werror','-Itools/test_mocks','-Isource','-Ilib/lz4','source/gangstar_lightmap_pack.c','lib/lz4/lz4.c',str(harness),'-lz','-pthread','-Wl,--wrap=malloc','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 cases=[blob[:10],b'BAD!'+blob[4:],blob[:8]+struct.pack('<I',129)+blob[12:],blob[:16]+struct.pack('<I',1)+blob[20:],blob[:28]+struct.pack('<I',0xffffffff)+blob[32:],blob[:-1]]
 bad=bytearray(blob);bad[32]^=1;cases.append(bad) # checksum mismatch
 bad=bytearray(blob);bad[36]^=255;cases.append(bad) # malformed compressed stream
 for bad in cases:
  pack.write_bytes(bad);subprocess.run([str(exe),'invalid'],check=True)
 pack.unlink();subprocess.run([str(exe),'missing'],check=True)
print('Lightmap pack: exact bytes, concurrent independent readers, allocation failure, bounds, corrupt index/payload/CRC, absent cache and errno preservation passed')
