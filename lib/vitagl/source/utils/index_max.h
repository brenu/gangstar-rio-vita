#ifndef RIO_INDEX_MAX_H
#define RIO_INDEX_MAX_H
#include <stdint.h>
#include <stddef.h>
/* Independent reduction lanes avoid a serial maximum dependency for every
 * index. Typed loops keep the format branch outside the scan. No overread. */
#define RIO_INDEX_MAX_FUNCTION(bits) \
static inline uint32_t rio_index_max##bits(const uint##bits##_t *p,size_t count) { \
 uint##bits##_t a=0,b=0,c=0,d=0;size_t i=0; \
 for(;count-i>=4;i+=4) { \
  if(p[i]>a){a=p[i];} if(p[i+1]>b){b=p[i+1];} \
  if(p[i+2]>c){c=p[i+2];} if(p[i+3]>d){d=p[i+3];} \
 } \
 uint32_t hi=a;if(b>hi)hi=b;if(c>hi)hi=c;if(d>hi)hi=d; \
 for(;i<count;++i)if(p[i]>hi)hi=p[i]; \
 return hi; \
}
RIO_INDEX_MAX_FUNCTION(16)
RIO_INDEX_MAX_FUNCTION(32)
#undef RIO_INDEX_MAX_FUNCTION
#endif
