#ifndef GANGSTAR_VERTEX_SCRATCH_POLICY_H
#define GANGSTAR_VERTEX_SCRATCH_POLICY_H
#include <stdint.h>
#include <stddef.h>
#define RIO_SCRATCH_RECORDS 256u
typedef struct {uintptr_t begin,end;} RioScratchRecord;
typedef struct {RioScratchRecord slots[RIO_SCRATCH_RECORDS];unsigned count,peak,overflow;} RioScratchRegistry;
static inline void rio_scratch_add(RioScratchRegistry *r,uintptr_t p,size_t bytes) {
 if(!p || !bytes)return;
 if(bytes>UINTPTR_MAX-p){r->overflow=1;return;}
 unsigned i;
 for(i=0;i<r->count;++i)if(r->slots[i].begin==p)break;
 if(i==RIO_SCRATCH_RECORDS){r->overflow=1;return;}
 r->slots[i]=(RioScratchRecord){p,p+bytes};
 if(i==r->count)++r->count;
 if(r->count>r->peak)r->peak=r->count;
}
static inline void rio_scratch_remove(RioScratchRegistry *r,uintptr_t p) {
 for(unsigned i=0;i<r->count;++i)if(r->slots[i].begin==p){r->slots[i]=r->slots[--r->count];return;}
}
static inline int rio_scratch_lookup(const RioScratchRegistry *r,uintptr_t p,uintptr_t *end) {
 if(r->overflow)return -1;
 for(unsigned i=0;i<r->count;++i)if(p>=r->slots[i].begin && p<r->slots[i].end){*end=r->slots[i].end;return 1;}
 return 0;
}
#endif
