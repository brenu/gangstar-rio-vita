#include "vertex_range.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
 uint16_t indices[128],rebased[128],original[128];
 unsigned char vertices[65536u*4u];
 for(unsigned i=0;i<sizeof(vertices);++i)vertices[i]=(unsigned char)(i^(i>>8));
 uint32_t first=999,span=999;
 assert(!rio_vertex_range16(NULL,1,&first,&span));
 assert(!rio_vertex_range16(indices,0,&first,&span));
 uint16_t low[]={0,2,1},small[]={255,255},wide[]={256,65535};
 assert(!rio_vertex_range16(low,3,&first,&span));
 assert(!rio_vertex_range16(small,2,&first,&span));
 assert(!rio_vertex_range16(wide,2,&first,&span));
 uint32_t measured_lo=77,measured_hi=88;
 assert(!rio_vertex_bounds16(NULL,1,&measured_lo,&measured_hi));
 assert(!rio_vertex_bounds16(indices,0,&measured_lo,&measured_hi));
 assert(measured_lo==77 && measured_hi==88);
 uint16_t full[]={0,65535};
 assert(rio_vertex_bounds16(full,2,&measured_lo,&measured_hi));
 assert(measured_lo==0 && measured_hi+1==65536);
 uint16_t edge[]={65535,65535};assert(rio_vertex_range16(edge,2,&first,&span));
 assert(first==65535 && span==1);
 unsigned seed=7;
 for(unsigned t=0;t<10000;++t) {
  unsigned base=256+(t%64000),width=1+(t%128);
  if(base+width>65536)width=65536-base;
  for(unsigned i=0;i<128;++i){seed=seed*1664525u+1013904223u;indices[i]=(uint16_t)(base+seed%width);}
  uint32_t lowest=65535,highest=0,measured_lo,measured_hi;
  for(unsigned i=0;i<128;++i){if(indices[i]<lowest)lowest=indices[i];if(indices[i]>highest)highest=indices[i];}
  assert(rio_vertex_bounds16(indices,128,&measured_lo,&measured_hi));
  assert(measured_lo==lowest && measured_hi==highest);
  memcpy(original,indices,sizeof(indices));
  assert(rio_vertex_range16(indices,128,&first,&span));
  assert(first>=base && first+span<=base+width);
  rio_vertex_rebase16(rebased,indices,128,first);
  assert(!memcmp(original,indices,sizeof(indices)));
  for(unsigned stride=1;stride<=4;++stride) {
   unsigned char copy[128*4];assert(span*stride<=sizeof(copy));
   memcpy(copy,vertices+first*stride,span*stride);
   for(unsigned i=0;i<128;++i){assert(rebased[i]<span);
    assert(!memcmp(copy+rebased[i]*stride,vertices+indices[i]*stride,stride));}
  }
 }
 puts("Vertex range: boundaries, 10000 randomized ranges, original indices unchanged and every fetched vertex byte preserved");
}
