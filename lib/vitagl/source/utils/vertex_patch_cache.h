#ifndef RIO_VERTEX_PATCH_CACHE_H
#define RIO_VERTEX_PATCH_CACHE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define RIO_VERTEX_PATCH_SLOTS 128
#define RIO_VERTEX_PATCH_BYTES 256
typedef struct {
 uintptr_t patcher,shader,source,output;
 unsigned program;size_t attributes_size,streams_size;
 unsigned char attributes[RIO_VERTEX_PATCH_BYTES],streams[RIO_VERTEX_PATCH_BYTES];
} RioVertexPatch;
typedef struct {RioVertexPatch slots[RIO_VERTEX_PATCH_SLOTS];unsigned hits,misses;} RioVertexPatchCache;
static inline void rio_vertex_patch_invalidate(RioVertexPatchCache *c,unsigned program) {
 RioVertexPatch *p=&c->slots[program%RIO_VERTEX_PATCH_SLOTS];
 if(p->program==program)p->output=0;
}
static inline int rio_vertex_patch_match(RioVertexPatchCache *c,unsigned program,uintptr_t patcher,
 uintptr_t shader,uintptr_t source,const void *attributes,size_t na,const void *streams,size_t ns,uintptr_t output) {
 RioVertexPatch *p=&c->slots[program%RIO_VERTEX_PATCH_SLOTS];
 int match=program && output && p->program==program && p->output==output && p->patcher==patcher &&
  p->shader==shader && p->source==source && na<=RIO_VERTEX_PATCH_BYTES && ns<=RIO_VERTEX_PATCH_BYTES &&
  na==p->attributes_size && ns==p->streams_size && (!na || !memcmp(p->attributes,attributes,na)) && (!ns || !memcmp(p->streams,streams,ns));
 if(match)++c->hits;else ++c->misses;return match;
}
static inline void rio_vertex_patch_store(RioVertexPatchCache *c,unsigned program,uintptr_t patcher,
 uintptr_t shader,uintptr_t source,const void *attributes,size_t na,const void *streams,size_t ns,uintptr_t output) {
 if(!program || !output || na>RIO_VERTEX_PATCH_BYTES || ns>RIO_VERTEX_PATCH_BYTES)return;
 RioVertexPatch *p=&c->slots[program%RIO_VERTEX_PATCH_SLOTS];
 p->program=program;p->patcher=patcher;p->shader=shader;p->source=source;p->output=output;
 p->attributes_size=na;p->streams_size=ns;if(na)memcpy(p->attributes,attributes,na);if(ns)memcpy(p->streams,streams,ns);
}
#endif
