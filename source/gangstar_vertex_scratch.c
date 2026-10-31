#include "gangstar_vertex_scratch.h"
#include "gangstar_vertex_scratch_policy.h"
#include "gangstar_startup_trampoline.h"
#include "utils/logger.h"
#include <so_util/so_util.h>
#include <kubridge.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
extern so_module so_mod;
static RioScratchRegistry registry,retired;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static atomic_int ready;
static unsigned additions,removals,hits,retired_hits;
static uintptr_t (*original_alloc)(uintptr_t,int);
static void (*original_release)(uintptr_t,uintptr_t);
void gangstar_vertex_snapshot_failure(unsigned reason,const void *source,size_t bytes,
 uintptr_t limit,unsigned stride,unsigned vertices) {
 static unsigned reports;
 if(reports++<4)l_warn("RIO_SNAPSHOT_FAILURE reason=%u source=%p bytes=%u limit=%p stride=%u vertices=%u",reason,source,(unsigned)bytes,(void *)limit,stride,vertices);
}
static uintptr_t tracked_process_alloc(uintptr_t heap,int bytes) {
 uintptr_t result=original_alloc(heap,bytes);
 if(result && bytes>0) {
  pthread_mutex_lock(&lock);rio_scratch_add(&registry,result,(size_t)bytes);++additions;pthread_mutex_unlock(&lock);
 }
 return result;
}
static void tracked_process_release(uintptr_t heap,uintptr_t pointer) {
 // Native render batching can retain the CPU pointer after its allocator
 // releases it. Keep only range metadata until frame end; never retain/free
 // the allocation itself. The draw snapshots the current valid GLES input.
 pthread_mutex_lock(&lock);
 const uintptr_t *bounds=(const uintptr_t *)heap;
 if(pointer<bounds[0] || pointer>=bounds[1]) {
  uintptr_t end=0;
  int found=rio_scratch_lookup(&registry,pointer,&end);
  if(found<0)retired.overflow=1;
  else if(found>0) {
   uintptr_t previous=0;
   if(rio_scratch_lookup(&retired,pointer,&previous)>0 && previous>end)end=previous;
   rio_scratch_add(&retired,pointer,end-pointer);
  }
 }
 rio_scratch_remove(&registry,pointer);++removals;pthread_mutex_unlock(&lock);
 original_release(heap,pointer);
}
int gangstar_vertex_extra_range(uintptr_t pointer,uintptr_t *end) {
 if(!atomic_load_explicit(&ready,memory_order_acquire))return -1;
 pthread_mutex_lock(&lock);
 int result=rio_scratch_lookup(&registry,pointer,end);
 if(!result){result=rio_scratch_lookup(&retired,pointer,end);if(result>0)++retired_hits;}
 if(result>0)++hits;
 pthread_mutex_unlock(&lock);return result;
}
void gangstar_vertex_scratch_frame_end(void) {
 pthread_mutex_lock(&lock);retired=(RioScratchRegistry){0};pthread_mutex_unlock(&lock);
}
void gangstar_vertex_scratch_report(void) {
 pthread_mutex_lock(&lock);
 unsigned live=registry.count,peak=registry.peak,overflow=registry.overflow,a=additions,r=removals,h=hits,rh=retired_hits;
 additions=removals=hits=retired_hits=0;pthread_mutex_unlock(&lock);
 l_info("RIO_VERTEX_SCRATCH_TRACK ready=%u live=%u peak=%u overflow=%u allocated=%u released=%u hits=%u retired_hits=%u",atomic_load(&ready),live,peak,overflow,a,r,h,rh);
}
void gangstar_vertex_scratch_install(void) {
 uintptr_t alloc=so_symbol(&so_mod,"_ZN6glitch4core18CProcessBufferHeap5allocEi");
 uintptr_t release=so_symbol(&so_mod,"_ZN6glitch4core18CProcessBufferHeap7releaseEPv");
 const uint32_t alloc_words[]={0xe92d4030,0xe5902000};
 const uint32_t release_words[]={0xe5903000,0xe1530001};
 uint32_t acode[6],rcode[6],literal[2]={0};
 if(!alloc || !release || memcmp((void *)alloc,alloc_words,8) || memcmp((void *)release,release_words,8) ||
    !rio_startup_trampoline(acode,alloc,alloc_words,literal) || !rio_startup_trampoline(rcode,release,release_words,literal)) {
  l_error("Vertex scratch tracking: unsupported native methods; copy-all fallback");return;
 }
 uintptr_t target=(so_mod.patch_head+3)&~(uintptr_t)3;
 size_t bytes=sizeof(acode)+sizeof(rcode);
 if(target<so_mod.patch_base || target>so_mod.patch_base+so_mod.patch_size || bytes>so_mod.patch_base+so_mod.patch_size-target) {
  l_error("Vertex scratch tracking: patch arena full; copy-all fallback");return;
 }
 so_mod.patch_head=target+bytes;
 memcpy((void *)target,acode,sizeof(acode));memcpy((void *)(target+sizeof(acode)),rcode,sizeof(rcode));
 kuKernelFlushCaches((void *)target,bytes);
 original_alloc=(void *)target;original_release=(void *)(target+sizeof(acode));
 hook_addr(alloc,(uintptr_t)tracked_process_alloc);kuKernelFlushCaches((void *)alloc,8);
 hook_addr(release,(uintptr_t)tracked_process_release);kuKernelFlushCaches((void *)release,8);
 atomic_store_explicit(&ready,1,memory_order_release);
 l_info("Vertex scratch tracking: native temporary allocations/releases installed");
}
