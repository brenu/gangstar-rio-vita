"""Exercise actual native wrappers against deterministic immutable OBB bytes."""
from pathlib import Path
import subprocess, os, re, struct
s=Path('source/gangstar_native_startup.c').read_text()
a=s.index('typedef struct {uintptr_t mapped;unsigned char *data;uint32_t offset;} RioPartialMap;')
b=s.index('static NativeCall wrappers[]',a)
code=s[a:b].replace('(uintptr_t)__builtin_return_address(0)','test_caller')
# Validate the whitelist against actual ARM call instructions in the supported ELF.
elf=Path('../work/libgl2jni.so').read_bytes()
sites=[int(x,16) for x in re.search(r'static const unsigned sites\[\]=\{([^}]+)',code)[1].split(',')]
for delta in sites:
 pc=0x475e24+delta-4
 word=struct.unpack_from('<I',elf,pc)[0]
 assert word>>24==0xeb
 imm=word&0xffffff
 if imm&0x800000:imm-=0x1000000
 assert pc+8+imm*4==0x38ca98
assert struct.unpack_from('<5I',elf,0xd393e8)[2:]==(128,128,128)
p=Path('work/tests/partial_lightmap.c')
p.write_text(r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gangstar_texture_lod.h"
#include "gangstar_district_manifest.h"
#define PHASE_BASE 47
typedef uintptr_t (*NativeCall)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
struct {NativeCall original;} probes[104];
static uintptr_t test_caller;
static int is_main=1,full_calls,draw_calls,fail_read,corrupt_header,bad_format;
static unsigned read_bytes,wanted_start;
static int gangstar_wait_is_main(void){return is_main;}
#define l_info(...) ((void)0)
static unsigned char header[128];
static unsigned char byte_at(unsigned pos){return (unsigned char)(pos*17u+pos/1024u);}
int gangstar_lightmap_read_range(uint32_t offset,void *dst,uint32_t start,uint32_t n){
 assert(offset==183416410u);assert(start<=2796344u && n<=2796344u-start);
 if(fail_read && start)return 0;
 for(unsigned i=0;i<n;++i)((unsigned char*)dst)[i]=start+i<128?header[start+i]:byte_at(start+i);
 if(corrupt_header && start==0)((unsigned char*)dst)[0]=0;
 read_bytes+=n;return 1;
}
static uintptr_t full(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){(void)a;(void)b;(void)c;(void)d;++full_calls;return 789;}
static unsigned mip(unsigned f,unsigned w,unsigned h,unsigned l,unsigned alpha){
 (void)f;assert(w==2048 && h==2048 && !alpha);return bad_format?1:(w>>l)*(h>>l)/2;
}
static uintptr_t draw(uintptr_t out,uintptr_t t,uintptr_t map,uintptr_t key,uintptr_t name,uintptr_t level,uintptr_t flag){
 assert(t && key==44 && name==55 && flag==77);assert(level==4 || level==0);
 uintptr_t *m=(uintptr_t*)map;assert(m[1]==2796344u);unsigned char *data=(void*)m[0];
 assert(!memcmp(data,header,128));
 for(unsigned i=wanted_start;i<2796344u;++i)assert(data[i]==byte_at(i));
 ++draw_calls;*(uintptr_t*)out=123;return out;
}
'''+code+r'''
int main(void){
 unsigned offsets[]={0,4,8,12,16,28,76,80,84,108};
 unsigned values[]={0x20534444,124,0x21007,2048,2048,12,32,4,0x30435445,0x401008};
 for(unsigned i=0;i<10;++i)rio_lod_put(header+offsets[i],values[i]);
 probes[PHASE_BASE+55].original=full;probes[PHASE_BASE+56].original=(NativeCall)draw;
 partial_ctor_base=0x10000;partial_mip_bytes=mip;
 unsigned char texture[208]={0};
 *(unsigned*)(texture+0x88)=11;*(unsigned*)(texture+0xbc)=2048;*(unsigned*)(texture+0xc0)=2048;*(unsigned*)(texture+0xb8)=2;
 uintptr_t map[3]={0},out=0;test_caller=partial_ctor_base+0x16c;
 const char *name="./copacabana_lightmap00.tga";
 // Every verified constructor site receives the same exact tail; seven arguments survive.
 const unsigned sites[]={0x16c,0x418,0x498,0x510,0x8f0,0xb9c,0xc1c,0xc94};
 for(unsigned i=0;i<8;++i){
  read_bytes=0;test_caller=partial_ctor_base+sites[i];
  assert(stream_partial_map((uintptr_t)map,(uintptr_t)name,0,0)==1);
  assert(!memcmp((void*)map[0],header,128) && map[1]==2796344u);
  wanted_start=2785408u;
  assert(stream_partial_mips((uintptr_t)&out,(uintptr_t)texture,(uintptr_t)map,44,55,4,77)==(uintptr_t)&out);
  assert(out==123 && read_bytes==11064u && !map[0] && !map[1]);
 }
 // A different requested level or incompatible native pixel format reads every byte.
 for(unsigned i=0;i<2;++i){
  bad_format=i;read_bytes=0;assert(stream_partial_map((uintptr_t)map,(uintptr_t)name,0,0)==1);
  wanted_start=128;stream_partial_mips((uintptr_t)&out,(uintptr_t)texture,(uintptr_t)map,44,55,i?4:0,77);
  assert(read_bytes==2796344u);
 }
 // Workers, unknown callers, unsupported sources and invalid DDS headers use native mapping.
 is_main=0;assert(stream_partial_map((uintptr_t)map,(uintptr_t)name,0,0)==789);is_main=1;
 test_caller=0;assert(stream_partial_map((uintptr_t)map,(uintptr_t)name,0,0)==789);
 test_caller=partial_ctor_base+0x16c;
 assert(stream_partial_map((uintptr_t)map,(uintptr_t)"car.tga",0,0)==789);
 corrupt_header=1;assert(stream_partial_map((uintptr_t)map,(uintptr_t)name,0,0)==789);corrupt_header=0;
 // Failed pixel reads never invoke the GPU-facing native consumer, and release ownership.
 assert(stream_partial_map((uintptr_t)map,(uintptr_t)name,0,0)==1);
 int before=draw_calls;fail_read=1;out=123;
 stream_partial_mips((uintptr_t)&out,(uintptr_t)texture,(uintptr_t)map,44,55,4,77);
 assert(draw_calls==before && !out && !map[0]);
 assert(full_calls==4);
}
''')
subprocess.run(['cc','-std=gnu11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DGANGSTAR_WORLD_TEXTURE_LOD','-DGANGSTAR_TEXTURE_LOD_EDGE=512','-I','source',str(p),'-o','work/tests/partial_lightmap'],check=True)
subprocess.run(['work/tests/partial_lightmap'],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('Partial lightmaps: original headers, exact tails across all constructor sites, full-read fallback, seven arguments, worker exclusion and failed-read cleanup passed under ASan/UBSan')
