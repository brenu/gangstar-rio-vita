"""Check native ABI evidence and forwarding around the nonblocking task guard."""
from pathlib import Path
import struct, subprocess, tempfile
elf=Path('../work/libgl2jni.so').read_bytes()
for address,words in {
 0x81e880:(0xe92d47f0,0xe5d0301c),
 0x7b2258:(0xe5937054,), # image count
 0x7b2264:(0xe5935058,), # image table
 0x7b2398:(0xe5937064,), # material count
 0x7b23a4:(0xe5935068,), # material table
 0x413de0:(0xe2800004,), # SFunction functor follows vtable
 0x80de68:(0xe49b3004,0xe1a00004,0xe5933000), # values -> SImage** -> SImage*
 0x46c0f8:(0xe351000d,), # game material factory recognizes type13 image refs
 0x80df2c:(0xe49b3004,0xe1a00004,0xe5933000), # second native image path uses same indirection
 0x80df60:(0xebfe95fa,), # calls constructImage for that second path
 0x81e8d0:(0xe5961008,), # list node's task pointer
}.items():
 assert struct.unpack_from('<'+'I'*len(words),elf,address)==words,hex(address)
s=Path('source/gangstar_native_startup.c').read_text()
a=s.index('#ifdef GANGSTAR_MATERIAL_PREFETCH\nstatic uintptr_t material_handler_vtable')
b=s.index('static uintptr_t stream_queued_highres(',a)
wrapper=s[a:b]
with tempfile.TemporaryDirectory(prefix='rio-material-wrapper-') as temp:
 p=Path(temp);c=p/'test.c'
 c.write_text(r'''
#include "gangstar_material_dispatch.h"
#include <assert.h>
typedef int pthread_mutex_t_bionic;
static int pthread_mutex_lock_soloader(pthread_mutex_t_bionic *p){(void)p;return 0;}
static int pthread_mutex_unlock_soloader(pthread_mutex_t_bionic *p){(void)p;return 0;}
static int gangstar_stream_defer_name(const char *s){(void)s;return 0;}
static int main_thread=1,forwarded;
static int gangstar_wait_is_main(void){return main_thread;}
static uintptr_t original(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){(void)a;(void)b;(void)c;assert(d==9);++forwarded;return 77;}
typedef uintptr_t (*NativeCall)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
struct {NativeCall original;} probes[59];
#define PHASE_BASE 0
''' + wrapper + r'''
int main(void){probes[58].original=original;
 // Guard failures forward without touching even an invalid native object.
 assert(material_dispatch(1,1,1,9)==77);
 main_thread=0;assert(material_dispatch(1,1,0,9)==77);main_thread=1;
 assert(material_dispatch(1,1,0,9)==77); // unresolved vtables/free callback
 assert(forwarded==3);return 0;
}
''')
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-DGANGSTAR_MATERIAL_PREFETCH','-Isource',str(c),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Material dispatch ABI signatures, table counts and blocking/worker/unresolved-symbol forwarding passed')
