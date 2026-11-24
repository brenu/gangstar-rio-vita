#include "../source/gangstar_lightmap_pool.c"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
void _log_print(int level,const char *format,...){(void)level;(void)format;}
static void *worker(void *id){
 for(unsigned i=0;i<20000;++i){unsigned char *p=gangstar_lightmap_pool_get(2796216);if(!p)continue;
  unsigned char tag=(unsigned char)((uintptr_t)id);
  p[0]=p[2796215]=tag;for(volatile int n=0;n<20;++n){}
  assert(p[0]==tag && p[2796215]==tag);assert(gangstar_lightmap_pool_put(p));}
 return NULL;
}
int main(void){
 int outside;void *p[RIO_LIGHTMAP_SLOTS];
 assert(!gangstar_lightmap_pool_get(2796216) && !gangstar_lightmap_pool_put(&outside) && !gangstar_lightmap_pool_put(NULL));
 gangstar_lightmap_pool_init();
 assert(!gangstar_lightmap_pool_get(699192) && !gangstar_lightmap_pool_get(RIO_LIGHTMAP_SLOT_BYTES+1));
 for(unsigned i=0;i<RIO_LIGHTMAP_SLOTS;++i){p[i]=gangstar_lightmap_pool_get(i&1?2796216:2796344);assert(p[i] && !((uintptr_t)p[i]&15));memset(p[i],(int)i,RIO_LIGHTMAP_SLOT_BYTES);}
 assert(!gangstar_lightmap_pool_get(2796216));
 for(unsigned i=0;i<RIO_LIGHTMAP_SLOTS;++i){assert(((unsigned char *)p[i])[RIO_LIGHTMAP_SLOT_BYTES-1]==i);}
 assert(gangstar_lightmap_pool_put(p[2]) && gangstar_lightmap_pool_get(2796216)==p[2]);
 assert(gangstar_lightmap_pool_owns(p[0]) && !gangstar_lightmap_pool_owns(&outside) && !gangstar_lightmap_pool_put(&outside));
 for(unsigned i=0;i<RIO_LIGHTMAP_SLOTS;++i)assert(gangstar_lightmap_pool_put(p[i]));
 pthread_t t[6];for(uintptr_t i=0;i<6;++i)pthread_create(&t[i],NULL,worker,(void *)(i+1));
 for(int i=0;i<6;++i)pthread_join(t[i],NULL);
 assert(!atomic_load(&used));
 puts("Lightmap pool: size class, exhaustion, reuse, foreign pointers and concurrent ownership passed");
}
