#ifndef GANGSTAR_SOURCE_CACHE_H
#define GANGSTAR_SOURCE_CACHE_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define RIO_SOURCE_CACHE_LIMIT (64u*1024u*1024u)
#define RIO_SOURCE_CACHE_SLOTS 384u
typedef struct {int archive,original;uint64_t offset;uint32_t source_size;} RioSourceKey;
typedef struct {RioSourceKey key;void *data;size_t size;uint64_t age;} RioSourceEntry;
typedef struct {RioSourceEntry entries[RIO_SOURCE_CACHE_SLOTS];size_t bytes;unsigned count,evictions;uint64_t age;} RioSourceCache;
static inline size_t rio_source_allowance(size_t cap,size_t cached,size_t used,size_t watermark,size_t reader) {
 size_t game=used>=cached?used-cached:used;
 if(game>=watermark || reader>=watermark-game)return 0;
 size_t available=watermark-game-reader;return available<cap?available:cap;
}
static inline void rio_source_evict(RioSourceCache *c) {
 unsigned oldest=RIO_SOURCE_CACHE_SLOTS;
 for(unsigned i=0;i<RIO_SOURCE_CACHE_SLOTS;++i)
  if(c->entries[i].data && (oldest==RIO_SOURCE_CACHE_SLOTS || c->entries[i].age<c->entries[oldest].age))oldest=i;
 if(oldest==RIO_SOURCE_CACHE_SLOTS)return;
 RioSourceEntry *e=&c->entries[oldest];c->bytes-=e->size;--c->count;++c->evictions;
 free(e->data);memset(e,0,sizeof(*e));
}
static inline void rio_source_trim(RioSourceCache *c,size_t limit) {while(c->bytes>limit)rio_source_evict(c);}
static inline RioSourceEntry *rio_source_find(RioSourceCache *c,RioSourceKey key) {
 for(unsigned i=0;i<RIO_SOURCE_CACHE_SLOTS;++i){RioSourceEntry *e=&c->entries[i];
  if(e->data && e->key.archive==key.archive && e->key.original==key.original &&
     e->key.offset==key.offset && e->key.source_size==key.source_size){e->age=++c->age;return e;}
 }
 return NULL;
}
// The native reader receives its own copy, never a borrowed cache allocation.
static inline void *rio_source_clone(RioSourceCache *c,RioSourceKey key,size_t *size) {
 RioSourceEntry *e=rio_source_find(c,key);if(!e)return NULL;
 void *copy=malloc(e->size);if(!copy)return NULL;
 memcpy(copy,e->data,e->size);*size=e->size;return copy;
}
static inline int rio_source_store(RioSourceCache *c,RioSourceKey key,const void *data,size_t size,size_t limit) {
 if(!data || !size || size>limit)return 0;
 if(rio_source_find(c,key))return 1;
 while(c->bytes>limit-size || c->count==RIO_SOURCE_CACHE_SLOTS)rio_source_evict(c);
 void *copy=malloc(size);if(!copy)return 0;memcpy(copy,data,size);
 for(unsigned i=0;i<RIO_SOURCE_CACHE_SLOTS;++i)if(!c->entries[i].data){
  c->entries[i]=(RioSourceEntry){key,copy,size,++c->age};c->bytes+=size;++c->count;return 1;
 }
 free(copy);return 0;
}
#endif
