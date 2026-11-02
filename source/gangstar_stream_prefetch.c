#include "gangstar_stream_prefetch.h"
#ifdef GANGSTAR_STREAM_PREFETCH
#include "gangstar_texture_lod.h"
#include "gangstar_district_manifest.h"
#include "utils/logger.h"
#include <pthread.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <string.h>
#include <stddef.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static int worker_started,worker_failed;
static struct {unsigned offset,size;uint64_t began;int state;char name[192];} job;
// Only copied immutable asset metadata crosses threads. Native texture/material
// objects are never retained or accessed by the worker.
static void *worker(void *unused) {
 (void)unused;sceKernelChangeThreadPriority(0,191);
 for(;;) {
  pthread_mutex_lock(&lock);
  while(job.state!=1)pthread_cond_wait(&changed,&lock);
  unsigned offset=job.offset,size=job.size;char name[192];memcpy(name,job.name,sizeof(name));
  pthread_mutex_unlock(&lock);
  uint64_t began=sceKernelGetProcessTimeWide();
  int retained=gangstar_texture_source_prepare(offset,size,name);
  uint64_t elapsed=sceKernelGetProcessTimeWide()-began;
  pthread_mutex_lock(&lock);job.state=retained?2:3;pthread_mutex_unlock(&lock);
  l_info("RIO_STREAM_PREPARE offset=%u retained=%d elapsed_us=%llu",offset,retained,(unsigned long long)elapsed);
 }
 return NULL;
}
static uintptr_t pointer_at(const void *p,size_t offset){uintptr_t v;memcpy(&v,(const char *)p+offset,sizeof(v));return v;}
int gangstar_stream_defer(const void *texture) {
 if(!texture)return 0;
 int state;memcpy(&state,(const char *)texture+0x84,sizeof(state));
 // Native manager retries state -1 and keeps the existing low-res texture.
 // Other states, missing low-res data and alpha reloads stay entirely native.
 if(state!=-1 || !pointer_at(texture,0x98) || *((const unsigned char *)texture+0x95))return 0;
 uintptr_t materials=pointer_at(texture,0xb0);
 if(!materials || materials==(uintptr_t)texture+0xb0)return 0;
 return gangstar_stream_defer_name((const char *)pointer_at(texture,0x78));
}
int gangstar_stream_defer_name(const char *name) {
 if(!name || strnlen(name,192)==192)return 0;
 const char *base=strrchr(name,'/');base=base?base+1:name;
 unsigned offset=0,size=0;
 for(unsigned i=0;i<sizeof(rio_district_assets)/sizeof(*rio_district_assets);++i)
  if(strstr(rio_district_assets[i].name,"_lightmap") && !strcmp(base,rio_district_assets[i].name)) {
   offset=rio_district_assets[i].offset;size=rio_district_assets[i].size;break;
  }
 if(!size || gangstar_texture_source_cached(offset,size))return 0;
 uint64_t now=sceKernelGetProcessTimeWide();
 pthread_mutex_lock(&lock);
 if(worker_failed){pthread_mutex_unlock(&lock);return 0;}
 if(!worker_started) {
  pthread_t thread;
  if(pthread_create(&thread,NULL,worker,NULL)){worker_failed=1;pthread_mutex_unlock(&lock);return 0;}
  pthread_detach(thread);worker_started=1;
 }
 if(job.state==1) {
  int defer=job.offset==offset && job.size==size && now-job.began<1500000u;
  pthread_mutex_unlock(&lock);return defer;
 }
 // A completed/failed job gets exactly one native attempt, even if its cache
 // entry was reclaimed before the next frame. Never loop on failed admission.
 if(job.state && job.offset==offset && job.size==size) {
  pthread_mutex_unlock(&lock);return 0;
 }
 job.offset=offset;job.size=size;job.began=now;job.state=1;
 memcpy(job.name,base,strlen(base)+1);
 pthread_cond_signal(&changed);pthread_mutex_unlock(&lock);
 l_info("RIO_STREAM_DEFER offset=%u bytes=%u",offset,size);
 return 1;
}
#endif
