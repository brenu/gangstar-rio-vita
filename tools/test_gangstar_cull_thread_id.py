"""Compile the actual cache wrapper and exercise isolation and invalidation."""
from pathlib import Path
import subprocess
s=Path('source/gangstar_native_startup.c').read_text();a=s.index('static struct {uintptr_t value;unsigned valid;uint64_t hits,fills,outside_hits;} frame_thread_id;');b=s.index('static int native_probe_needed(',a)
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
typedef uintptr_t (*NativeCall)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static unsigned cull_prepare_depth;
static _Thread_local int is_main;
static _Thread_local uintptr_t thread_value;
static atomic_uint native_calls;
static int gangstar_wait_is_main(void){return is_main;}
static uintptr_t native_id(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){assert(a==1 && b==2 && c==3 && d==4);atomic_fetch_add(&native_calls,1);return thread_value;}
'''
main=r'''
static void *worker(void *arg){thread_value=(uintptr_t)arg;
 for(unsigned i=0;i<10000;++i)assert(sequential_id_call(native_id,1,2,3,4)==thread_value);
 return NULL;
}
int main(void){
 is_main=0;thread_value=7;
 for(unsigned i=0;i<10;++i)assert(sequential_id_call(native_id,1,2,3,4)==7);
 assert(native_calls==10 && !frame_thread_id.valid);
 is_main=1;cull_prepare_depth=1;
 pthread_t workers[4];for(uintptr_t i=0;i<4;++i)assert(!pthread_create(&workers[i],NULL,worker,(void *)(100+i)));
 for(unsigned i=0;i<10000;++i)assert(sequential_id_call(native_id,1,2,3,4)==7);
 for(unsigned i=0;i<4;++i)assert(!pthread_join(workers[i],NULL));
 assert(native_calls==40011 && frame_thread_id.fills==1 && frame_thread_id.hits==9999);
 cull_prepare_depth=0;
 assert(sequential_id_call(native_id,1,2,3,4)==7 && frame_thread_id.outside_hits==1);
 // A new frame can bind a different main-thread/native ID.
 sequential_id_frame_begin();thread_value=8;
 assert(sequential_id_call(native_id,1,2,3,4)==8);
 assert(frame_thread_id.fills==2);
 sequential_id_frame_begin();thread_value=0;
 assert(sequential_id_call(native_id,1,2,3,4)==0 && !frame_thread_id.valid);
 assert(sequential_id_call(native_id,1,2,3,4)==0 && !frame_thread_id.valid);
}
'''
p=Path('work/tests/frame_thread_id.c');p.write_text(prefix+s[a:b]+main)
for suffix,flags in [('',[]),('_sanitized',['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
 out='work/tests/frame_thread_id'+suffix
 subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-pthread',*flags,str(p),'-o',out],check=True)
 import os
 subprocess.run([out],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Culling thread ID: actual native forwarding, one native initialization per frame, worker isolation across40000lookups, main-frame scope outside culling, reset and zero-value fallback (ASan/UBSan)')
