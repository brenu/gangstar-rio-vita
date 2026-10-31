#ifndef GANGSTAR_VERTEX_WATCH_H
#define GANGSTAR_VERTEX_WATCH_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define RIO_VERTEX_WATCH_CAPACITY 1024u
typedef struct {
 const unsigned char *source;const char *attribute;
 unsigned draw,index,bytes;unsigned char before[16];
} RioVertexWatchEntry;
typedef struct {
 uintptr_t begin,end;unsigned count,dropped;RioVertexWatchEntry entries[RIO_VERTEX_WATCH_CAPACITY];
} RioVertexWatch;
static inline void rio_vertex_watch_add(RioVertexWatch *w,const char *attribute,unsigned draw,
 unsigned index,const void *source,unsigned bytes) {
 uintptr_t p=(uintptr_t)source;
 if(!w->begin || w->end<=w->begin || !bytes || bytes>16 || p<w->begin || p>=w->end || bytes>w->end-p)return;
 if(w->count==RIO_VERTEX_WATCH_CAPACITY){++w->dropped;return;}
 RioVertexWatchEntry *e=&w->entries[w->count++];
 e->source=source;e->attribute=attribute;e->draw=draw;e->index=index;e->bytes=bytes;
 memcpy(e->before,source,bytes);
}
static inline void rio_vertex_watch_report(RioVertexWatch *w,FILE *out,int range_still_mapped) {
 unsigned changed=0;
 if(range_still_mapped)for(unsigned i=0;i<w->count;++i) {
  const RioVertexWatchEntry *e=&w->entries[i];
  if(!memcmp(e->source,e->before,e->bytes))continue;
  ++changed;fprintf(out,"source_changed draw=%u attribute=%s index=%u pointer=%p before=",e->draw,e->attribute,e->index,(const void *)e->source);
  for(unsigned j=0;j<e->bytes;++j)fprintf(out,"%02x",e->before[j]);
  fprintf(out," after=");for(unsigned j=0;j<e->bytes;++j)fprintf(out,"%02x",e->source[j]);
  fprintf(out,"\n");
 }
 fprintf(out,"source_watch samples=%u changed=%u dropped=%u range_valid=%d\n",w->count,changed,w->dropped,range_still_mapped);
 w->count=w->dropped=0;
}
#endif
