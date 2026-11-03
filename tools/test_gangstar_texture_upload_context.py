"""Verify the native upload identity scope, ABI, and Release exclusion."""
from pathlib import Path
import subprocess,struct,os
s=Path('source/gangstar_native_startup.c').read_text()
elf=Path('../work/libgl2jni.so').read_bytes()
assert struct.unpack_from('<II',elf,0x882568)==(0xe92d4ff0,0xe1a06000)
assert struct.unpack_from('<I',elf,0x75d7dc)[0]==0xe480c008
assert struct.unpack_from('<I',elf,0x75da0c)[0]==0xe5940008
a=s.index('static uintptr_t texture_upload_context(');b=s.index('// Only the verified',a)
wrapper=s[a:b]
a=s.index('static int native_probe_needed(');b=s.index('// Borrow only while',a);gate=s[a:b]
code=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#define PHASE_BASE 47
#define RIO_PHASE_NATIVE_TID 46
#define RIO_PHASE_SUBMIT_BASE 47
#define l_info(...) ((void)0)
typedef uintptr_t (*NativeCall)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static struct {NativeCall original;} probes[108];
static int main_thread=1,called;
static unsigned reuse_frame;
static struct {uintptr_t text_base;} so_mod;
static const char *scope;
static int gangstar_wait_is_main(void){return main_thread;}
static const char *gangstar_texture_upload_scope(const char *name){const char *old=scope;scope=name;return old;}
static uintptr_t original(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){
 assert(a && b==1 && c==2 && d==3);++called;
#if defined(GANGSTAR_UPLOAD_STAGES) && !defined(GANGSTAR_RELEASE_BUILD)
 if(main_thread){const char *name;memcpy(&name,(const char *)a+8,sizeof(name));assert(scope==(name && strnlen(name,192)<192?name:NULL));}
#endif
 return 0x12345678;
}
''' + wrapper + gate + r'''
int main(void){
 unsigned char texture[128]={0};const char *name="_packetc/car_cops_texture_d.tga",*outer="outer";
 memcpy(texture+8,&name,sizeof(name));probes[PHASE_BASE+60].original=original;scope=outer;
 assert(texture_upload_context((uintptr_t)texture,1,2,3)==0x12345678 && called==1 && scope==outer);
 main_thread=0;assert(texture_upload_context((uintptr_t)texture,1,2,3)==0x12345678 && scope==outer);
 main_thread=1;char oversized[193];memset(oversized,'x',sizeof(oversized));name=oversized;memcpy(texture+8,&name,sizeof(name));
 assert(texture_upload_context((uintptr_t)texture,1,2,3)==0x12345678 && scope==outer);
 name=NULL;memcpy(texture+8,&name,sizeof(name));assert(texture_upload_context((uintptr_t)texture,1,2,3)==0x12345678 && scope==outer);
#if defined(GANGSTAR_UPLOAD_STAGES) && !defined(GANGSTAR_RELEASE_BUILD)
 assert(native_probe_needed(PHASE_BASE+60));
#else
 assert(!native_probe_needed(PHASE_BASE+60));
#endif
}
'''
p=Path('work/tests/texture_upload_context.c');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(code)
for flags in ([],['-DGANGSTAR_UPLOAD_STAGES'],['-DGANGSTAR_UPLOAD_STAGES','-DGANGSTAR_RELEASE_BUILD']):
    subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined','-fno-omit-frame-pointer',*flags,str(p),'-o','work/tests/texture_upload_context'],check=True)
    subprocess.run(['work/tests/texture_upload_context'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
print('Texture upload identity: native prologue/string offsets verified; actual forwarding, borrowed scope, worker/null/oversized-name handling and Release exclusion passed (ASan/UBSan)')
