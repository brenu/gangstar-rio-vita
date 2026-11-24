#include "../source/gangstar_spill_pool.c"
#include "gangstar_gpu_pools.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
void _log_print(int level,const char *format,...){(void)level;(void)format;}
static void *worker(void *id){
 for(unsigned i=0;i<20000;++i){unsigned char *p=gangstar_spill_get(16,2097152);if(!p)continue;
  unsigned char tag=(unsigned char)((uintptr_t)id);
  p[0]=p[2097151]=tag;for(volatile int n=0;n<20;++n){}
  assert(p[0]==tag && p[2097151]==tag);assert(gangstar_spill_put(p));}
 return NULL;
}
static int same(RioGpuPools p,unsigned ram,unsigned physical){return p.ram_mib==ram && p.physical_mib==physical;}
int main(void){
 const size_t mib=1024u*1024u;int outside;void *p[RIO_SPILL_SLOTS];
 assert(!gangstar_spill_get(16,2097152) && !gangstar_spill_put(&outside) && !gangstar_spill_put(NULL) && !gangstar_spill_usable(NULL));
 gangstar_spill_init();
 assert(!gangstar_spill_get(16,RIO_SPILL_MIN_BYTES-1) && !gangstar_spill_get(16,RIO_SPILL_SLOT_BYTES+1) && !gangstar_spill_get(8192,2097152));
 // Every level prefix of a 2048x2048 chain seen failing on hardware fits a slot.
 size_t seen[]={2097152,2621440,2752512,2785280,2793472,2796216};
 for(unsigned i=0;i<sizeof(seen)/sizeof(*seen);++i){void *q=gangstar_spill_get(4096,seen[i]);assert(q && !((uintptr_t)q&4095));assert(gangstar_spill_usable(q)==RIO_SPILL_SLOT_BYTES && gangstar_spill_put(q));}
 for(unsigned i=0;i<RIO_SPILL_SLOTS;++i){p[i]=gangstar_spill_get(16,2796216);assert(p[i]);memset(p[i],(int)i,RIO_SPILL_SLOT_BYTES);}
 assert(!gangstar_spill_get(16,2097152));
 for(unsigned i=0;i<RIO_SPILL_SLOTS;++i)assert(((unsigned char *)p[i])[RIO_SPILL_SLOT_BYTES-1]==i);
 assert(gangstar_spill_usable((char *)p[1]+100)==RIO_SPILL_SLOT_BYTES-100);
 assert(!gangstar_spill_usable(pool-1) && !gangstar_spill_usable(pool+RIO_SPILL_SLOTS*RIO_SPILL_SLOT_BYTES));
 assert(gangstar_spill_put(p[1]) && gangstar_spill_get(16,2097152)==p[1]);
 for(unsigned i=0;i<RIO_SPILL_SLOTS;++i)assert(gangstar_spill_put(p[i]));
 pthread_t t[6];for(uintptr_t i=0;i<6;++i)pthread_create(&t[i],NULL,worker,(void *)(i+1));
 for(int i=0;i<6;++i)pthread_join(t[i],NULL);
 assert(!atomic_load(&used));
 assert(same(rio_gpu_pools(38797312,27262976),24,23));   // usual console
 assert(same(rio_gpu_pools(110100480,18874368),24,15));  // 18 MiB of phycont
 assert(same(rio_gpu_pools(38797312,14*mib),24,11) && same(rio_gpu_pools(38797312,14*mib-1),16,8));
 assert(same(rio_gpu_pools(32*mib-1,27262976),16,8));
 for(size_t f=14;f<64;++f){RioGpuPools g=rio_gpu_pools(64*mib,f*mib);assert(g.physical_mib<=23 && g.physical_mib>=11 && g.physical_mib+3<=f);}
 puts("Spill pool and GPU pool sizing: limits, exhaustion, reuse, foreign pointers and concurrent ownership passed");
}
