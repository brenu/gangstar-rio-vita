"""Exercise the actual Pak entry wrapper for frame and image-loader threads."""
from pathlib import Path
import subprocess,os
s=Path('source/gangstar_native_startup.c').read_text()
a=s.index('static uintptr_t probe_20(uintptr_t a,');b=s.index('// Bounded, main-thread-only history.',a)
# Only widen the adjacent pair of native 32-bit pointer fields for the host.
wrapper=s[a:b].replace('b + 0x24','b + 0x28')
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gangstar_texture_lod.h"
#include "gangstar_pak_lookup.h"
#define l_info(...) ((void)0)
typedef uintptr_t (*Call)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static struct {Call original;} probes[21];
static uintptr_t direct_file_vtable;
static uintptr_t (*make_memory_file)(uintptr_t,const void*,uint32_t,const char*,int);
static int main_thread=1,native_calls,reduced_calls,prefetch_calls,fail_read,fail_factory;
static const char *path="main.12030.com.gameloft.android.ANMP.GloftG4HM.obb";
static int gangstar_wait_is_main(void){return main_thread;}
static const char *file_name(uintptr_t p){assert(p);return path;}
static uintptr_t original(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){assert(b && !c && d==99);++native_calls;return a;}
const void *gangstar_texture_original_get(int a,uint64_t b,uint32_t c,const char*d,uint32_t*e){(void)a;(void)b;(void)c;(void)d;(void)e;assert(!"No original lightmap admission");return NULL;}
const void *gangstar_texture_lod_get(int a,uint64_t b,uint32_t c,const char*d,uint32_t*e){assert(a==0 && b==100 && c==2796344 && rio_lod_car_name(d));++reduced_calls;if(fail_read)return NULL;*e=699192;return malloc(*e);}
static void gangstar_archive_prefetch_asset(int a,uint64_t b,uint32_t c){assert(main_thread && a==0 && b==100 && c==2796344);++prefetch_calls;}
static uintptr_t factory(uintptr_t a,const void *p,uint32_t n,const char*name,int owned){assert(p && n==699192 && rio_lod_car_name(name) && owned);if(!fail_factory){*(uintptr_t*)a=123;free((void*)p);}return a;}
''' + wrapper + r'''
int main(void){
 uintptr_t vt[16]={0},file=(uintptr_t)vt,out=0;unsigned char reader[64]={0};
 RioPakEntry entry={100,2796344,"_packetc/car_cops_texture_damage_d.tga",0};
 *(uintptr_t*)(reader+8)=(uintptr_t)&file;*(uintptr_t*)(reader+0x20)=(uintptr_t)&entry;
 *(uintptr_t*)(reader+0x28)=(uintptr_t)&entry+16;
 vt[0x28/4]=(uintptr_t)file_name;direct_file_vtable=(uintptr_t)vt;
 make_memory_file=factory;probes[20].original=original;
 main_thread=0;probe_20((uintptr_t)&out,(uintptr_t)reader,0,99);
#ifdef GANGSTAR_CAR_TEXTURE_LOD
 assert(out==123 && reduced_calls==1 && !native_calls && !prefetch_calls);
 out=0;entry.name="_packetc/copacabana_lightmap00.tga";probe_20((uintptr_t)&out,(uintptr_t)reader,0,99);
 assert(!out && reduced_calls==1 && native_calls==1);
 entry.name="_packetc/car_cops_texture_damage_d.tga";fail_read=1;
 probe_20((uintptr_t)&out,(uintptr_t)reader,0,99);assert(native_calls==2 && !out);fail_read=0;
 fail_factory=1;probe_20((uintptr_t)&out,(uintptr_t)reader,0,99);assert(native_calls==3 && !out);fail_factory=0;
 entry.extra=0x10000;probe_20((uintptr_t)&out,(uintptr_t)reader,0,99);assert(native_calls==4);entry.extra=0;
 path="other.obb";probe_20((uintptr_t)&out,(uintptr_t)reader,0,99);assert(native_calls==5);path="main.12030.com.gameloft.android.ANMP.GloftG4HM.obb";
 main_thread=1;probe_20((uintptr_t)&out,(uintptr_t)reader,0,99);assert(out==123 && !prefetch_calls);
#else
 assert(!out && native_calls==1 && !reduced_calls && !prefetch_calls);
#endif
}
'''
p=Path('work/tests/car_reader.c');p.write_text(code)
for flags in ([],['-DGANGSTAR_CAR_TEXTURE_LOD'],['-DGANGSTAR_CAR_TEXTURE_LOD','-DGANGSTAR_UPLOAD_STAGES'],['-DGANGSTAR_CAR_TEXTURE_LOD','-DGANGSTAR_RELEASE_BUILD']):
 subprocess.run(['cc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-DGANGSTAR_SMALL_LIGHTMAPS','-DGANGSTAR_WORLD_TEXTURE_LOD','-DGANGSTAR_TEXTURE_LOD_EDGE=512','-fsanitize=address,undefined','-fno-omit-frame-pointer',*flags,'-Isource',str(p),'-o','work/tests/car_reader'],check=True)
 subprocess.run(['work/tests/car_reader'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
print('Car readers: actual Pak wrapper preserves owned readers on frame/worker threads; exact allowlist, original-OBB guard, failure ownership, world-worker exclusion and flag-off fallback pass (ASan/UBSan)')
