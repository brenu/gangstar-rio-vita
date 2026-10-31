#ifndef RIO_VERTEX_RANGE_H
#define RIO_VERTEX_RANGE_H
#include <stdint.h>
#include <stddef.h>
/* Only select ranges saving at least half the old vertex prefix, and at
 * least 256 vertices, to amortize the extra index-buffer write. */
static inline int rio_vertex_bounds16(const uint16_t *indices,size_t count,
 uint32_t *lowest,uint32_t *highest) {
 if(!indices || !count)return 0;
 uint32_t lo=65535,hi=0;
 for(size_t i=0;i<count;++i) {
  uint32_t v=indices[i];if(v<lo)lo=v;if(v>hi)hi=v;
 }
 *lowest=lo;*highest=hi;return 1;
}
static inline int rio_vertex_range_worth_copying(uint32_t lo,uint32_t hi) {
 return lo>=256 && lo>=hi-lo+1;
}
static inline int rio_vertex_range16(const uint16_t *indices,size_t count,
 uint32_t *first,uint32_t *vertices) {
 uint32_t lo,hi;
 if(!rio_vertex_bounds16(indices,count,&lo,&hi) || !rio_vertex_range_worth_copying(lo,hi))return 0;
 *first=lo;*vertices=hi-lo+1;return 1;
}
static inline void rio_vertex_rebase16(uint16_t *out,const uint16_t *in,
 size_t count,uint32_t first) {
 for(size_t i=0;i<count;++i)out[i]=(uint16_t)(in[i]-first);
}
#endif
