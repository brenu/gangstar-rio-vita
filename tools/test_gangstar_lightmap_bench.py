"""Run the actual optional startup benchmark against synthetic sparse inputs."""
from pathlib import Path
import subprocess,tempfile,struct,zlib,ctypes
root=Path.cwd()
with tempfile.TemporaryDirectory(prefix='rio-lightmap-bench-') as temp:
 d=Path(temp);payload=(b'DDS '+bytes(range(256))*11000)[:2796344];assert len(payload)==2796344
 packed=zlib.compress(payload,1);blob=bytearray(b'RLB1'+struct.pack('<I',2))
 with (d/'main.12030.com.gameloft.android.ANMP.GloftG4HM.obb').open('wb') as f:
  for offset in [183416410,806291654]:
   f.seek(offset);f.write(payload)
   blob+=struct.pack('<IIII',offset,len(payload),len(packed),zlib.crc32(payload))+packed
 pack=d/'lightmap-benchmark.bin';pack.write_bytes(blob)
 harness=d/'main.c';harness.write_text('''
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
uint64_t sceKernelGetProcessTimeWide(void){static uint64_t tick;return ++tick;}
void _log_print(int level,const char *format,...){(void)level;va_list a;va_start(a,format);vprintf(format,a);puts("");va_end(a);}
void gangstar_lightmap_benchmark(void);
int main(void){gangstar_lightmap_benchmark();}
''')
 executable=d/'test'
 subprocess.run(['cc','-std=c11','-D_XOPEN_SOURCE=700','-O2','-Wall','-Wextra','-Werror','-DGANGSTAR_LIGHTMAP_BENCH','-DDEBUG_SOLOADER',f'-DDATA_PATH="{d}/"','-Itools/test_mocks','-Isource','-Ilib/lz4','source/gangstar_lightmap_bench.c','lib/lz4/lz4.c',str(harness),'-lz','-o',str(executable)],check=True)
 def run():return subprocess.check_output([str(executable)],text=True)
 out=run();assert out.count('exact=1')==6 and 'finished=1' in out
 # Generate LZ4 test input using the same vendored library as the Vita build.
 shared=d/'libtestlz4.so'
 subprocess.run(['cc','-O2','-shared','-fPIC','lib/lz4/lz4.c','-o',str(shared)],check=True)
 library=ctypes.CDLL(str(shared));library.LZ4_compressBound.argtypes=[ctypes.c_int]
 library.LZ4_compress_default.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int]
 target=ctypes.create_string_buffer(library.LZ4_compressBound(len(payload)))
 n=library.LZ4_compress_default(payload,target,len(payload),len(target));assert n>0
 packed=target.raw[:n];blob=bytearray(b'RLB2'+struct.pack('<I',2))
 for offset in [183416410,806291654]:
  blob+=struct.pack('<IIII',offset,len(payload),len(packed),zlib.crc32(payload))+packed
 pack.write_bytes(blob);out=run();assert out.count('codec=lz4')==6 and 'finished=1' in out
 for bad in [blob[:9],b'BAD!'+blob[4:],blob[:16]+struct.pack('<I',0xffffffff)+blob[20:],blob[:27]]:
  pack.write_bytes(bad);assert 'finished=0' in run()
 bad=bytearray(blob);bad[28]^=255;pack.write_bytes(bad);assert 'finished=0' in run()
 pack.unlink();assert run()==''
print('Lightmap benchmark: zlib and LZ4 alternating measurements, byte equality, malformed/truncated/corrupt inputs, allocation bounds and absent-input skip passed')
