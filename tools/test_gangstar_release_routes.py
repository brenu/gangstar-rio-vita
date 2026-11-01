"""Run production archive routing with profiling timestamps disabled."""
from pathlib import Path
import subprocess
s=Path('source/gangstar_native_startup.c').read_text();a=s.index('static uintptr_t route_dispatch(');b=s.index('\n}\n#endif\nstatic uintptr_t probe_15',a)+2
p=Path('work/tests/release_routes.c')
p.write_text(r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include "gangstar_route_cache.h"
#define GANGSTAR_DIRECT_ASSET_PATHS
#define GANGSTAR_AUDIO_DIRECT_PATHS
struct {uintptr_t load_addr;} so_mod={0x98000000};
typedef uintptr_t (*Call)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
struct {Call original;} probes[21];
static uintptr_t pak_vtable=1;
static RioRouteCache route_cache;
static struct Capture {uintptr_t reader,file;char resolved[RIO_ROUTE_PATH];} *capture;
static unsigned route_skipped,route_packed_misses,route_hits,route_entry_hits,route_failed,route_misses,route_learned;
static atomic_uint audio_path_opened,audio_path_missing,audio_path_fallback;
static uint64_t path_us,path_worst;
static int main_thread=1,audio,handled=1,direct_calls,native_calls,loose_calls;
static void *path_indexes=(void*)1,*audio_path_indexes=(void*)2;
static int gangstar_wait_is_main(void){return main_thread;}
static int rio_audio_scope_active(void){return audio;}
static uint64_t rio_diagnostic_clock(void){return 0;}
static int route_stamp(uintptr_t parent,uint64_t *stamp,int *packed){assert(parent==123);*stamp=456;*packed=1;return 1;}
static int reader_present(uintptr_t p,uintptr_t r){(void)p;(void)r;return 0;}
static uintptr_t original(uintptr_t out,uintptr_t parent,uintptr_t name,uintptr_t extra){(void)parent;(void)name;(void)extra;++native_calls;*(uintptr_t*)out=0;return out;}
static uintptr_t loose(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){++loose_calls;return original(a,b,c,d);}
static uintptr_t probe_20(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){return original(a,b,c,d);}
static int direct_path_open(uintptr_t out,uintptr_t parent,const char *name,uintptr_t extra,uint64_t stamp,void *indexes,RioRouteCache *cache){
 assert(parent==123 && name && extra==0 && stamp==456);++direct_calls;
 assert(main_thread ? (indexes==path_indexes && cache==&route_cache) : (indexes==audio_path_indexes && !cache));
 *(uintptr_t*)out=handled?789:0;return handled;
}
'''+s[a:b]+r'''
int main(void){
 probes[14].original=loose;probes[15].original=original;probes[16].original=original;probes[20].original=original;
 uintptr_t out=0,caller=so_mod.load_addr+0x467b7c;
 route_dispatch((uintptr_t)&out,123,(uintptr_t)"lightmap.tga",0,caller,0);
 assert(out==789 && direct_calls==1 && !native_calls); // main-thread fast path with clock zero
 handled=0;route_dispatch((uintptr_t)&out,123,(uintptr_t)"absent.tga",0,caller,0);
 assert(!out && native_calls==1);
 route_dispatch((uintptr_t)&out,123,(uintptr_t)"absent.tga",0,caller,0);
 assert(loose_calls==1 && route_packed_misses==1); // preserve negative lookup cache
 main_thread=0;audio=1;handled=1;
 route_dispatch((uintptr_t)&out,123,(uintptr_t)"music.wav",0,caller,0);
 assert(out==789 && atomic_load(&audio_path_opened)==1);
 audio=0;route_dispatch((uintptr_t)&out,123,(uintptr_t)"other.wav",0,caller,0);
 assert(!out && native_calls==3); // unknown worker retains original routing
}
''')
subprocess.run(['cc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-Isource',str(p),'-o','work/tests/release_routes'],check=True)
subprocess.run(['work/tests/release_routes'],check=True)
print('Release archive routing: actual function keeps direct immutable-OBB opens and negative caching with zero profiling timestamps, audio worker direct opens and ordinary-worker fallback')
