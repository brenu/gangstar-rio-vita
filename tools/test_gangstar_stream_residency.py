"""Read-only native streaming snapshot, including lock order and copied rows."""
from pathlib import Path
import subprocess,os,struct
s=Path('source/gangstar_native_startup.c').read_text()
a=s.index('static void stream_residency_snapshot(uintptr_t manager) {');b=s.index('\nstatic struct { char name',a)
body=s[a:b].replace('manager+0xc4','manager+0xc0').replace('object+0x9c','object+0xa0')
# Widen native pointer fields for the 64-bit host without overlapping neighbors.
elf=Path('../work/libgl2jni.so').read_bytes()
for at,word in {0x4793e0:0xe59590c4,0x4793e4:0xe595b0c8,0x477928:0xe5904084,0x477948:0xe1d638b0,0x477ac8:0xe596709c}.items():
 assert struct.unpack_from('<I',elf,at)[0]==word
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
static int main_thread=1,depth,records;static unsigned reuse_frame=123;
static uintptr_t manager,object;
static char name[128]="./mine_lightmap00.tga";
static void lock(void *p){assert(p==(void *)(depth?object+0x48:manager+12));++depth;}
static void unlock(void *p){assert(depth && p==(void *)(depth==2?object+0x48:manager+12));if(!--depth)strcpy(name,"changed");}
static void (*stream_census_lock)(void*)=lock,(*stream_census_unlock)(void*)=unlock;
static int gangstar_wait_is_main(void){return main_thread;}
static void texture_manager_residency_snapshot(void){assert(!depth);}
static void capture(const char *fmt,...){
 assert(!depth);va_list ap;va_start(ap,fmt);assert(va_arg(ap,unsigned)==123);
 assert(va_arg(ap,void*)==(void*)object);assert(va_arg(ap,unsigned)==3);
 assert(va_arg(ap,int)==1);assert(va_arg(ap,void*));assert(va_arg(ap,unsigned)==4);
 assert(va_arg(ap,unsigned)==77);assert(!strcmp(va_arg(ap,const char*),"./mine_lightmap00.tga"));va_end(ap);++records;
}
#define l_info(...) capture(__VA_ARGS__)
''' + body + r'''
int main(void){
 uintptr_t m[64]={0},o[64]={0},h[32]={0},vec[1]={(uintptr_t)o};
 manager=(uintptr_t)m;object=(uintptr_t)o;
 *(uintptr_t*)(manager+0xc0)=(uintptr_t)vec;*(uintptr_t*)(manager+0xc8)=(uintptr_t)(vec+1);
 *(uintptr_t*)(object+0x78)=(uintptr_t)name;*(unsigned short*)(object+0x80)=3;
 *(int*)(object+0x84)=1;*(uintptr_t*)(object+0xa0)=(uintptr_t)h;
 *(unsigned*)((char*)h+4)=4;*(unsigned*)((char*)h+0x60)=77;
 uintptr_t before[64];memcpy(before,o,sizeof(o));
 main_thread=0;for(unsigned i=0;i<600;++i)stream_residency_snapshot(manager);
 assert(!records && !depth);main_thread=1;
 for(unsigned i=0;i<300;++i)stream_residency_snapshot(manager);
#if defined(GANGSTAR_UPLOAD_STAGES) && !defined(GANGSTAR_RELEASE_BUILD)
 assert(records==1 && !strcmp(name,"changed"));
#else
 assert(!records && !strcmp(name,"./mine_lightmap00.tga"));
#endif
 assert(!depth && !memcmp(before,o,sizeof(o)));
}
'''
p=Path('work/tests/stream_residency.c');p.write_text(code)
for flags in ([],['-DGANGSTAR_UPLOAD_STAGES'],['-DGANGSTAR_UPLOAD_STAGES','-DGANGSTAR_RELEASE_BUILD']):
 subprocess.run(['cc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined','-fno-omit-frame-pointer',*flags,str(p),'-o','work/tests/stream_residency'],check=True)
 subprocess.run(['work/tests/stream_residency'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
print('Stream residency: native offsets verified, main-thread sampling, manager/texture lock order, copied rows emitted after unlock, unchanged objects and Release exclusion pass (ASan/UBSan)')
