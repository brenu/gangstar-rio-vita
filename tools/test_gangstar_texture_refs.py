"""Ordinary native texture-manager census, with no retained native pointers."""
from pathlib import Path
import subprocess,os,struct
s=Path('source/gangstar_native_startup.c').read_text()
a=s.index('static void texture_manager_residency_snapshot(void) {');b=s.index('\nstatic void stream_residency_snapshot',a)
body=s[a:b].replace('manager+0x1c','manager+0x20').replace('manager+0xbc','manager+0xb8').replace('object+0x24','object+0x28')
elf=Path('../work/libgl2jni.so').read_bytes()
for at,word in {0x75c32c:0xe5902018,0x75c330:0xe590101c,0x75c354:0xe28460d0,0x791978:0xe5903004,0x79197c:0xe3530001}.items():assert struct.unpack_from('<I',elf,at)[0]==word
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
static uintptr_t manager,object;static int locked,records;static unsigned reuse_frame=456;
static char name[128]="./mine_lightmap00.tga";
static uintptr_t get(void){return manager;}
static uintptr_t (*texture_census_manager)(void)=get;
static void lock(void*p){assert(!locked && p==(void*)(manager+0xd0));locked=1;}
static void unlock(void*p){assert(locked && p==(void*)(manager+0xd0));locked=0;strcpy(name,"changed");}
static void (*stream_census_lock)(void*)=lock,(*stream_census_unlock)(void*)=unlock;
static void capture(const char*fmt,...){
 assert(!locked);va_list ap;va_start(ap,fmt);assert(va_arg(ap,unsigned)==456);
 assert(va_arg(ap,void*)==(void*)object);assert(va_arg(ap,unsigned)==3);assert(va_arg(ap,unsigned)==99);
 assert(va_arg(ap,unsigned)==137);assert(va_arg(ap,unsigned)==1);assert(va_arg(ap,unsigned)==1);
 assert(!strcmp(va_arg(ap,const char*),"./mine_lightmap00.tga"));va_end(ap);++records;
}
#define l_info(...) capture(__VA_ARGS__)
''' + body + r'''
int main(void){
 uintptr_t m[64]={0},o[32]={0},other[32]={0},vec[3]={(uintptr_t)o,0,(uintptr_t)other},queue[1]={(uintptr_t)o};
 manager=(uintptr_t)m;object=(uintptr_t)o;
 *(uintptr_t*)(manager+0x18)=(uintptr_t)vec;*(uintptr_t*)(manager+0x20)=(uintptr_t)(vec+3);
 *(uintptr_t*)(manager+0xb8)=(uintptr_t)queue;*(uintptr_t*)(manager+0xc0)=(uintptr_t)(queue+1);
 *(uintptr_t*)(object+8)=(uintptr_t)name;*(unsigned*)(object+4)=3;*(unsigned char*)(object+0x38)=137;
 *(unsigned*)(object+0x60)=99;*(uintptr_t*)(object+0x28)=1;
 *(uintptr_t*)((char*)other+8)=(uintptr_t)"./ordinary_car.tga";
 uintptr_t before[32];memcpy(before,o,sizeof(o));texture_manager_residency_snapshot();
#if defined(GANGSTAR_UPLOAD_STAGES) && !defined(GANGSTAR_RELEASE_BUILD)
 assert(records==1 && !strcmp(name,"changed"));
#else
 assert(!records && !strcmp(name,"./mine_lightmap00.tga"));
#endif
 assert(!locked && !memcmp(before,o,sizeof(o)));
}
'''
p=Path('work/tests/texture_refs.c');p.write_text(code)
for flags in ([],['-DGANGSTAR_UPLOAD_STAGES'],['-DGANGSTAR_UPLOAD_STAGES','-DGANGSTAR_RELEASE_BUILD']):
 subprocess.run(['cc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined','-fno-omit-frame-pointer',*flags,str(p),'-o','work/tests/texture_refs'],check=True)
 subprocess.run(['work/tests/texture_refs'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
print('Texture references: native table/mutex/refcount offsets verified; lightmap-only copied rows, null slots, marked queue, unlock-before-log, unchanged objects and Release exclusion pass (ASan/UBSan)')
