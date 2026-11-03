"""Exercise native seven-argument car mip selection without changing objects."""
from pathlib import Path
import subprocess,struct,os
s=Path('source/gangstar_native_startup.c').read_text();a=s.index('static uintptr_t stream_partial_mips(');b=s.index('static NativeCall wrappers[]',a)
elf=Path('../work/libgl2jni.so').read_bytes()
# Native method loads level from the incoming fifth stack-byte slot, uses it
# in 2^(max-level), and skips that many mip byte ranges using original sizes.
checks={0x478120:0xe59d6074,0x478128:0xe5951088,0x478130:0xe0663001,
        0x478184:0xe59520c0,0x478190:0xe59510bc,0x478278:0xe59b4004,
        0x478288:0xe0674004,0x47829c:0xe0821007}
for at,word in checks.items():assert struct.unpack_from('<I',elf,at)[0]==word
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gangstar_texture_lod.h"
#define PHASE_BASE 47
#define l_info(...) ((void)0)
typedef uintptr_t (*NativeCall)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static struct {NativeCall original;} probes[104];
typedef struct {uintptr_t mapped;unsigned char *data;uint32_t offset;} RioPartialMap;
static RioPartialMap partial_maps[16];
static unsigned (*partial_mip_bytes)(unsigned,unsigned,unsigned,unsigned,unsigned);
static unsigned reuse_frame;
static int main_thread=1,bad_format;
static uintptr_t expected_level,expected_args[7];
static unsigned calls;
static int gangstar_wait_is_main(void){return main_thread;}
int gangstar_lightmap_read_range(uint32_t o,void *p,uint32_t s,uint32_t n){(void)o;(void)p;(void)s;(void)n;assert(!"No deferred lightmap in this fixture");return 0;}
static unsigned mip(unsigned f,unsigned w,unsigned h,unsigned l,unsigned alpha){(void)f;assert(w==2048 && h==2048 && !alpha);return bad_format?1:((w>>l)*(h>>l)/2);}
static uintptr_t native(uintptr_t o,uintptr_t t,uintptr_t m,uintptr_t k,uintptr_t n,uintptr_t l,uintptr_t f){
 assert(o==expected_args[0] && t==expected_args[1] && m==expected_args[2] && k==expected_args[3] && n==expected_args[4] && f==expected_args[6] && l==expected_level);
 ++calls;return 0x12345678;
}
''' + s[a:b] + r'''
static void invoke(uintptr_t *args,uintptr_t chosen){
 memcpy(expected_args,args,7*sizeof(*args));expected_level=chosen;
 assert(stream_partial_mips(args[0],args[1],args[2],args[3],args[4],args[5],args[6])==0x12345678);
}
int main(void){
 probes[PHASE_BASE+56].original=(NativeCall)native;partial_mip_bytes=mip;
 unsigned char texture[208]={0},header[128]={0};
 unsigned offsets[]={0,4,8,12,16,28,76,80,84,108},values[]={0x20534444,124,0x21007,2048,2048,12,32,4,0x30435445,0x401008};
 for(unsigned i=0;i<10;++i)rio_lod_put(header+offsets[i],values[i]);
 *(unsigned *)(texture+0x88)=11;*(unsigned *)(texture+0xbc)=2048;*(unsigned *)(texture+0xc0)=2048;
 uintptr_t map[3]={(uintptr_t)header,2796344,0x5555},out=0;
 const char *name="./car_cops_texture_damage_d.tga";
 uintptr_t args[7]={(uintptr_t)&out,(uintptr_t)texture,(uintptr_t)map,77,(uintptr_t)&name,0,99};
 unsigned char before[sizeof(texture)],header_before[128];uintptr_t map_before[3];
 memcpy(before,texture,sizeof(before));memcpy(header_before,header,128);memcpy(map_before,map,sizeof(map));
#ifdef GANGSTAR_CAR_TEXTURE_LOD
 invoke(args,1);
#else
 invoke(args,0);
#endif
 assert(!memcmp(before,texture,sizeof(before)) && !memcmp(header_before,header,128) && !memcmp(map_before,map,sizeof(map)));
 args[5]=4;invoke(args,4);args[5]=1;invoke(args,1);args[5]=0;
 name="./copacabana_lightmap00.tga";invoke(args,0);name="other/car_cops_texture_damage_d.tga";invoke(args,0);
 name="./car_cops_texture_damage_d.tga";map[1]=699192;invoke(args,0);map[1]=2796344;
 header[0]=0;invoke(args,0);header[0]='D';
 bad_format=1;invoke(args,0);bad_format=0;
 *(unsigned *)(texture+0xbc)=1024;invoke(args,0);*(unsigned *)(texture+0xbc)=2048;
 main_thread=0;invoke(args,0);main_thread=1;
 uintptr_t value=args[4];args[4]=0;invoke(args,0);args[4]=value;
 value=args[1];args[1]=0;invoke(args,0);args[1]=value;
 name=NULL;invoke(args,0);
 assert(calls==13);
}
'''
p=Path('work/tests/car_stream.c');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(prefix)
for flags in ([],['-DGANGSTAR_CAR_TEXTURE_LOD'],['-DGANGSTAR_CAR_TEXTURE_LOD','-DGANGSTAR_UPLOAD_STAGES'],['-DGANGSTAR_CAR_TEXTURE_LOD','-DGANGSTAR_RELEASE_BUILD']):
    subprocess.run(['cc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-cast-function-type','-Wno-unused-variable','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DGANGSTAR_WORLD_TEXTURE_LOD','-DGANGSTAR_TEXTURE_LOD_EDGE=512',*flags,'-Isource',str(p),'-o','work/tests/car_stream'],check=True)
    subprocess.run(['work/tests/car_stream'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
print('Car streams: native level/dimension/offset instructions verified; actual seven-argument forwarding, guarded level-1 selection, unchanged readers/objects, lightmap/worker/invalid-layout/flag-off fallbacks passed (ASan/UBSan)')
