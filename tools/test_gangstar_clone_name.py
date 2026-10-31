from pathlib import Path
import subprocess,os
s=Path('source/gangstar_native_startup.c').read_text();a=s.index('static uintptr_t clone_name_data;');b=s.index('static uintptr_t phase_call(',a)
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
typedef uintptr_t (*NativeCall)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static struct {unsigned refs;char text[8];} name={1,"road"},other={1,"other"};
static unsigned native_gets;
static uintptr_t native_get(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){(void)a;(void)b;(void)c;(void)d;++native_gets;return 99;}
'''
main=r'''
static uintptr_t native_clone(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){(void)a;(void)c;(void)d;
 if(b)return submit_native_call(7,native_get,b,1,0,0);
 return submit_native_call(7,native_get,(uintptr_t)name.text,1,0,0);
}
int main(void){unsigned char material[64]={0};uintptr_t ptr=(uintptr_t)&name;memcpy(material+28,&ptr,sizeof(ptr));
 assert(submit_native_call(3,native_clone,(uintptr_t)material,0,0,0)==ptr);
 assert(name.refs==2 && clone_name_hits==1 && !clone_name_data && !native_gets);
 --name.refs;assert(name.refs==1);
 assert(submit_native_call(7,native_get,(uintptr_t)name.text,1,0,0)==99 && native_gets==1);
 assert(submit_native_call(3,native_clone,(uintptr_t)material,(uintptr_t)other.text,0,0)==99);
 assert(native_gets==2 && name.refs==1 && !clone_name_data);
 clone_name_data=ptr;
 assert(submit_native_call(7,native_get,(uintptr_t)other.text,1,0,0)==99);
 assert(submit_native_call(7,native_get,(uintptr_t)name.text,0,0,0)==99);
 assert(name.refs==1);
 clone_name_data=0;
 for(unsigned i=0;i<10000;++i){assert(submit_native_call(3,native_clone,(uintptr_t)material,0,0,0)==ptr);--name.refs;}
 assert(name.refs==1 && !clone_name_data);
}
'''
p=Path('work/tests/clone_name.c');p.write_text(prefix+s[a:b]+main)
for flags in ([],['-fsanitize=address,undefined','-fno-omit-frame-pointer']):
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,str(p),'-o','work/tests/clone_name'],check=True)
 subprocess.run(['work/tests/clone_name'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Clone name: exact string identity, atomic owned references, scope expiry, explicit-name and no-create fallback, 10000 balanced clone lifetimes (ASan/UBSan)')
