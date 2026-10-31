#include "index_max.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
int main(void) {
 assert(rio_index_max16(NULL,0)==0 && rio_index_max32(NULL,0)==0);
 uint32_t seed=7;
 for(size_t n=1;n<=4097;++n) {
  /* Exact-sized allocations let ASan detect any vector/tail overread. */
  uint16_t *a=malloc((n+1)*sizeof(*a));uint32_t *b=malloc((n+1)*sizeof(*b));assert(a && b);
  uint32_t hi16=0,hi32=0;
  for(size_t i=0;i<n;++i) {
   seed=seed*1664525u+1013904223u;a[i+1]=(uint16_t)seed;b[i+1]=seed;
   if(a[i+1]>hi16)hi16=a[i+1];
   if(b[i+1]>hi32)hi32=b[i+1];
  }
  assert(rio_index_max16(a+1,n)==hi16 && rio_index_max32(b+1,n)==hi32);
  for(unsigned lane=0;lane<4 && lane<n;++lane) {
   a[lane+1]=UINT16_MAX;b[lane+1]=UINT32_MAX;
   assert(rio_index_max16(a+1,n)==UINT16_MAX && rio_index_max32(b+1,n)==UINT32_MAX);
  }
  free(a);free(b);
 }
 puts("Index maxima: exact U16/U32 maxima, every tail length through4097, independent lanes, non-vector-aligned pointers and maximum integer values");
}
