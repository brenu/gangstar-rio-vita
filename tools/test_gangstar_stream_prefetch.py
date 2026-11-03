"""Exercise real queued-read guard/worker and native forwarding wrapper."""
from pathlib import Path
import tempfile,subprocess,struct,re
elf=Path('../work/libgl2jni.so').read_bytes()
assert struct.unpack_from('<II',elf,0x4770d4)==(0xe1a02000,0xe92d41f0)
s=Path('source/gangstar_native_startup.c').read_text();a=s.index('static uintptr_t stream_queued_highres(');b=s.index('static uintptr_t texture_upload_context(',a);wrapper=s[a:b]
with tempfile.TemporaryDirectory(prefix='rio-queued-') as temp:
 d=Path(temp);headers=d/'psp2/kernel';headers.mkdir(parents=True)
 (headers/'threadmgr.h').write_text('int sceKernelChangeThreadPriority(int,int);\n')
 (headers/'processmgr.h').write_text('#include <stdint.h>\nuint64_t sceKernelGetProcessTimeWide(void);\n')
 h=d/'main.c';h.write_text(r'''
#include "gangstar_stream_prefetch.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <time.h>
static atomic_ullong now=1000000;
static atomic_int cached,entered,release_job,succeed=1,create_failed,prepare_calls;
uint64_t sceKernelGetProcessTimeWide(void){return atomic_load(&now);}
int sceKernelChangeThreadPriority(int a,int b){(void)a;(void)b;return 0;}
void _log_print(int t,const char *f,...){(void)t;(void)f;}
int __real_pthread_create(pthread_t *,const pthread_attr_t *,void *(*)(void *),void *);
int __wrap_pthread_create(pthread_t *t,const pthread_attr_t *a,void *(*f)(void *),void *v){if(create_failed)return EAGAIN;return __real_pthread_create(t,a,f,v);}
int gangstar_texture_source_cached(uint32_t offset,uint32_t size){(void)offset;(void)size;return cached;}
int gangstar_texture_source_prepare(uint32_t offset,uint32_t size,const char *name){
 assert(size==2796344 && ((offset==183416410 && !strcmp(name,"copacabana_lightmap00.tga")) || (offset==806291654 && !strcmp(name,"southfavela_lightmap00.tga"))));
 atomic_fetch_add(&prepare_calls,1);entered=1;
 struct timespec delay={0,1000000};while(!release_job)nanosleep(&delay,NULL);
 cached=succeed;return succeed;
}
#include "gangstar_stream_prefetch.c"
typedef uintptr_t (*NativeCall)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
struct {NativeCall original;} probes[58];
#define PHASE_BASE 0
static int is_main=1,forwarded;
static const char *fault_scope;
static const char *gangstar_texture_fault_scope(const char *name){const char *old=fault_scope;fault_scope=name;return old;}
static int gangstar_wait_is_main(void){return is_main;}
static uintptr_t native(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){assert(a && c==3 && d==4);(void)b;++forwarded;return 77;}
''' + wrapper + r'''
static void ptr(unsigned char *texture,unsigned offset,uintptr_t value){memcpy(texture+offset,&value,sizeof(value));}
static void setup(unsigned char *texture,const char *name){memset(texture,0,256);int state=-1;memcpy(texture+0x84,&state,4);ptr(texture,0x78,(uintptr_t)name);ptr(texture,0x98,1);ptr(texture,0xb0,1);}
static void wait_enter(void){struct timespec delay={0,1000000};for(unsigned i=0;i<3000 && !entered;++i)nanosleep(&delay,NULL);assert(entered);}
static void wait_done(void){struct timespec delay={0,1000000};for(unsigned i=0;i<3000;++i){pthread_mutex_lock(&lock);int done=job.state!=1;pthread_mutex_unlock(&lock);if(done)return;nanosleep(&delay,NULL);}assert(!"worker timeout");}
int main(int argc,char **argv){
 (void)argv;unsigned char texture[256],other[256];probes[57].original=native;
 setup(texture,"_packetc/copacabana_lightmap00.tga");
 if(argc>1){create_failed=1;assert(!gangstar_stream_defer(texture));assert(!gangstar_stream_defer(texture));return 0;}
 assert(!gangstar_stream_defer(NULL));ptr(texture,0x98,0);assert(!gangstar_stream_defer(texture));
 setup(texture,"huds.bmp");assert(!gangstar_stream_defer(texture));
 setup(texture,"copacabana_lightmap00.tga");texture[0x95]=1;assert(!gangstar_stream_defer(texture));texture[0x95]=0;
 ptr(texture,0xb0,(uintptr_t)texture+0xb0);assert(!gangstar_stream_defer(texture));ptr(texture,0xb0,1);
 cached=1;assert(!gangstar_stream_defer(texture));cached=0;assert(!worker_started);
 is_main=0;assert(stream_queued_highres((uintptr_t)texture,0,3,4)==77);is_main=1;
 assert(stream_queued_highres((uintptr_t)texture,1,3,4)==77);assert(!worker_started);
 assert(stream_queued_highres((uintptr_t)texture,0,3,4)==0);wait_enter();assert(prepare_calls==1);
 setup(other,"southfavela_lightmap00.tga");assert(!gangstar_stream_defer(other));
 assert(gangstar_stream_defer(texture));atomic_fetch_add(&now,2000000);assert(!gangstar_stream_defer(texture));
 memset(texture,0,256);release_job=1;wait_done(); // worker uses copied metadata, not the object.
 setup(texture,"copacabana_lightmap00.tga");assert(!gangstar_stream_defer(texture));
 cached=0;assert(!gangstar_stream_defer(texture));assert(prepare_calls==1); // no retry loop after eviction.
 succeed=0;entered=0;assert(gangstar_stream_defer(other));wait_enter();wait_done();
 assert(!gangstar_stream_defer(other));assert(prepare_calls==2);
 assert(stream_queued_highres((uintptr_t)other,0,3,4)==77 && forwarded==3);
 assert(!fault_scope);
}
''')
 exe=d/'test';subprocess.run(['cc','-std=c11','-D_XOPEN_SOURCE=700','-O2','-Wall','-Wextra','-Werror','-DGANGSTAR_UPLOAD_STAGES','-DGANGSTAR_STREAM_PREFETCH','-DDEBUG_SOLOADER','-I'+str(d),'-Isource',str(h),'-pthread','-Wl,--wrap=pthread_create','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=10);subprocess.run([str(exe),'thread-failure'],check=True,timeout=10)
print('Queued stream: exact native signature, low-res/state/material guards, no worker native pointers, one job, timeout/failure fallback and original forwarding passed')
