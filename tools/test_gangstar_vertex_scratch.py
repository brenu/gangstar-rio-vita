from pathlib import Path
import subprocess,os
s=Path('source/gangstar_vertex_scratch.c').read_text();a=s.index('static uintptr_t tracked_process_alloc(');b=s.index('\nvoid gangstar_vertex_scratch_report(',a)
prefix=r'''
#include "gangstar_vertex_scratch_policy.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>
static RioScratchRegistry registry,retired;
static uintptr_t mock_heap[2]={0x1000,0x2000};
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static unsigned additions,removals,hits,retired_hits;
static atomic_int ready=1;
static atomic_uint allocated,freed;
static uintptr_t mock_alloc(uintptr_t heap,int bytes){assert(heap==(uintptr_t)mock_heap && bytes>0);atomic_fetch_add(&allocated,1);return (uintptr_t)malloc((size_t)bytes);}
static void mock_release(uintptr_t heap,uintptr_t p){assert(heap==(uintptr_t)mock_heap);uintptr_t end=0;pthread_mutex_lock(&lock);assert(!rio_scratch_lookup(&registry,p,&end));pthread_mutex_unlock(&lock);atomic_fetch_add(&freed,1);free((void *)p);}
static uintptr_t (*original_alloc)(uintptr_t,int)=mock_alloc;
static void (*original_release)(uintptr_t,uintptr_t)=mock_release;
'''
main=r'''
static void *worker(void *unused){(void)unused;for(unsigned i=0;i<4000;++i){
 int bytes=32+(int)(i%128);uintptr_t p=tracked_process_alloc((uintptr_t)mock_heap,bytes),end=0;assert(p);
 pthread_mutex_lock(&lock);assert(rio_scratch_lookup(&registry,p+(unsigned)bytes-1,&end)==1 && end==p+(unsigned)bytes);pthread_mutex_unlock(&lock);
 tracked_process_release((uintptr_t)mock_heap,p);
}return NULL;}
int main(void){
 RioScratchRegistry r={0};uintptr_t end=0;
 rio_scratch_add(&r,0x1000,64);assert(r.count==1 && rio_scratch_lookup(&r,0x103f,&end)==1 && end==0x1040);
 assert(!rio_scratch_lookup(&r,0x1040,&end));rio_scratch_remove(&r,0x1000);assert(!rio_scratch_lookup(&r,0x1000,&end));
 rio_scratch_add(&r,0x1000,16);rio_scratch_add(&r,0x1000,32);assert(r.count==1 && rio_scratch_lookup(&r,0x101f,&end)==1 && end==0x1020);
 r=(RioScratchRegistry){0};
 for(unsigned i=0;i<RIO_SCRATCH_RECORDS;++i)rio_scratch_add(&r,0x10000+i*1024,512);
 assert(!r.overflow && r.count==RIO_SCRATCH_RECORDS);
 rio_scratch_add(&r,0x800000,8);assert(r.overflow && rio_scratch_lookup(&r,0x10000,&end)==-1);
 r=(RioScratchRegistry){0};rio_scratch_add(&r,UINTPTR_MAX-3,8);assert(r.overflow);
 // Released overflow buffers remain classified for draws later this frame.
 uintptr_t q=tracked_process_alloc((uintptr_t)mock_heap,48);
 tracked_process_release((uintptr_t)mock_heap,q);
 assert(rio_scratch_lookup(&retired,q+47,&end)==1 && end==q+48);
 assert(gangstar_vertex_extra_range(q+47,&end)==1 && retired_hits==1);
 gangstar_vertex_scratch_frame_end();assert(!gangstar_vertex_extra_range(q,&end));
 additions=removals=0;atomic_store(&allocated,0);atomic_store(&freed,0);
 pthread_t threads[4];for(unsigned i=0;i<4;++i)assert(!pthread_create(&threads[i],NULL,worker,NULL));
 for(unsigned i=0;i<4;++i)assert(!pthread_join(threads[i],NULL));
 assert(registry.count==0 && !registry.overflow && registry.peak<=4 && additions==16000 && removals==16000);
 assert(atomic_load(&allocated)==16000 && atomic_load(&freed)==16000);
}
'''
p=Path('work/tests/vertex_scratch.c');p.write_text(prefix+s[a:b]+main)
for sanitize in (False,True):
 cmd=['cc','-std=c11','-O1' if sanitize else '-O2','-Wall','-Wextra','-Werror','-pthread','-I','source']
 if sanitize:cmd+=['-fsanitize=address,undefined']
 subprocess.run(cmd+[str(p),'-o','work/tests/vertex_scratch'],check=True)
 subprocess.run(['work/tests/vertex_scratch'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Native scratch: actual alloc/release forwarding, live and retired range lookup, frame-end expiry, registration before use, removal before free, exact boundaries, pointer reuse, overflow fallback and 16000 concurrent lifetimes passed (ASan/UBSan)')
