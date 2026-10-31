#include "gangstar_district_preload.h"
#ifdef GANGSTAR_DISTRICT_PRELOAD
#include "gangstar_district_policy.h"
#include "gangstar_district_manifest.h"
#include "gangstar_texture_lod.h"
#include "utils/logger.h"
#include <pthread.h>
#include <so_util/so_util.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <unistd.h>
extern so_module so_mod;
#define ASSETS (sizeof(rio_district_assets)/sizeof(*rio_district_assets))
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static unsigned jobs[384],count,cursor,epoch,current_mask,next_mask;
static int started;
static uint64_t last_poll;
static float previous[3];
static void *(*env_get)(void);
static void *worker(void *unused) {
 (void)unused;sceKernelChangeThreadPriority(0,191);
 unsigned active_epoch=0,processed=0,retained=0;
 for(;;) {
  pthread_mutex_lock(&lock);
  while(cursor==count)pthread_cond_wait(&changed,&lock);
  unsigned i=jobs[cursor++],version=epoch;
  pthread_mutex_unlock(&lock);
  const char *name=rio_district_assets[i].name;
  if(active_epoch!=version){active_epoch=version;processed=retained=0;}
  retained+=gangstar_texture_source_background(rio_district_assets[i].offset,rio_district_assets[i].size,name);++processed;
  // Yield between assets; cancellation replaces all unstarted work immediately.
  sceKernelDelayThread(5000);
  pthread_mutex_lock(&lock);int complete=version==epoch && cursor==count;pthread_mutex_unlock(&lock);
  if(complete)l_info("RIO_DISTRICT_DONE generation=%u processed=%u retained_at_completion=%u",version,processed,retained);
 }
 return NULL;
}
static unsigned zone_at(void *manager,const float position[3]) {
 uintptr_t first=*(uintptr_t *)((char *)manager+0x20),end=*(uintptr_t *)((char *)manager+0x24);
 if(end<first || (end-first)>4096 || ((first|end)&3) || (!first && end!=first))return 0;
 unsigned result=0;
 for(uintptr_t at=first;at<end;at+=4) {
  unsigned char *zone=*(unsigned char **)at;if(!zone)continue;
  const char *name=*(const char **)(zone+0x30);
  if(!name || strnlen(name,192)==192)continue;
  unsigned mask=rio_district_name(name,1);if(!mask)continue;
  float bounds[6];memcpy(bounds,zone+0x88,sizeof(bounds));
  if(rio_district_contains(bounds,position))result|=mask;
 }
 return result;
}
void gangstar_district_poll(void) {
 if(!started)return;
 uint64_t now=sceKernelGetProcessTimeWide();
 if(last_poll && now-last_poll<500000)return;
 void *manager=env_get();if(!manager)return;
 float position[3],future[3];memcpy(position,(char *)manager+0x2c,sizeof(position));
 for(unsigned i=0;i<3;++i)if(!isfinite(position[i]))return;
 rio_district_predict(position,previous,last_poll?(now-last_poll)/1000000.0f:0,future);
 memcpy(previous,position,sizeof(previous));last_poll=now;
 future[2]=position[2]; // World X/Y are horizontal; do not project above zone ceilings.
 float halfway[3]={(position[0]+future[0])*0.5f,(position[1]+future[1])*0.5f,position[2]};
 unsigned current=zone_at(manager,position),next=(zone_at(manager,future)|zone_at(manager,halfway))&~current;
 next&=~RIO_D_INTERIOR;
 if(current&RIO_D_INTERIOR)return; // Broad interior groups cannot identify a specific safehouse.
 if(!current)return; // Unknown/interior states retain the preceding safe plan.
 if(current==current_mask && next==next_mask)return;
 current_mask=current;next_mask=next;
 unsigned plan[384],priorities[384],used[ASSETS];memset(used,0,sizeof(used));
 unsigned total=0;size_t bytes=0,shared=0;
 // Base lightmaps first within each district, then remaining maps/atlases.
 for(unsigned pass=0;pass<6;++pass)for(unsigned i=0;i<ASSETS;++i) {
  const char *name=rio_district_assets[i].name;
  if(used[i] || !rio_lod_name(name))continue;
  unsigned district=rio_district_name(name,0);int original=rio_lod_original_source(name);
  size_t length=strlen(name);int base=length>=6 && !strcmp(name+length-6,"00.tga");
  int shared_map=rio_district_shared_map(name);
  int needed_original=original && (base || district==RIO_D_COPA);
  int eligible=pass==0?shared_map:
   pass==1?(district&current)&&needed_original:pass==2?(district&next)&&needed_original:
   pass==3?(district&current)&&!original:pass==4?!district&&!original:(district&next)&&!original;
  if(!eligible)continue;
  size_t size=original?rio_district_assets[i].size:174904u;
  if((pass==0 || pass==4) && shared+size>12u*1024u*1024u)continue;
  if(bytes+size>60u*1024u*1024u || total==384)continue;
  plan[total]=i;priorities[total++]=(pass==1 || pass==3)?4:(pass==0 || pass==4)?3:2;used[i]=1;bytes+=size;
  if(pass==0 || pass==4)shared+=size;
 }
 // Priority changes are metadata-only; native readers never borrow cache data.
 RioTextureSourcePriority desired[384];
 for(unsigned j=0;j<total;++j) {
  unsigned i=plan[j];desired[j]=(RioTextureSourcePriority){rio_district_assets[i].offset,
   rio_district_assets[i].size,rio_lod_original_source(rio_district_assets[i].name),priorities[j]};
 }
 gangstar_texture_source_priorities(desired,total);
 pthread_mutex_lock(&lock);memcpy(jobs,plan,total*sizeof(*plan));count=total;cursor=0;++epoch;
 unsigned version=epoch;pthread_cond_signal(&changed);pthread_mutex_unlock(&lock);
 l_info("RIO_DISTRICT_PLAN generation=%u current=0x%x upcoming=0x%x sources=%u bytes=%u shared_bytes=%u x=%.1f y=%.1f z=%.1f",
  version,current,next,total,(unsigned)bytes,(unsigned)shared,position[0],position[1],position[2]);
}
int gangstar_district_start(void) {
 if(started)return 1;
 uintptr_t get=so_symbol(&so_mod,"_ZN10EnvZoneMgr3GetEv");
 uintptr_t lookup=so_symbol(&so_mod,"_ZN10EnvZoneMgr7GetZoneEPKc");
 uintptr_t update=so_symbol(&so_mod,"_ZN10EnvZoneMgr6UpdateEiRKN6glitch4core8vector3dIfEE");
 uintptr_t contains=so_symbol(&so_mod,"_ZN7EnvZone8ContainsERKN6glitch4core8vector3dIfEE");
 static const uint32_t signature[]={0xed907a22,0xedd17a00};
 static const uint32_t lookup_sig[]={0xe92d41f0,0xe5908024};
 static const uint32_t update_sig[]={0xe92d45f0,0xed2d8b02};
 if(!get || !lookup || !update || memcmp((void *)lookup,lookup_sig,8) || memcmp((void *)update,update_sig,8) || !contains || memcmp((void *)contains,signature,8)) {
  l_warn("RIO_DISTRICT unavailable: environment layout signature mismatch");return 0;
 }
 env_get=(void *)get;
 pthread_t thread;
 if(pthread_create(&thread,NULL,worker,NULL))return 0;
 pthread_detach(thread);started=1;gangstar_district_poll();
 l_info("RIO_DISTRICT enabled cache_limit_mib=64 desired_limit_mib=60 horizon_seconds=8");return 1;
}

#else
int gangstar_district_start(void) {return 0;}
void gangstar_district_poll(void) {}
#endif
