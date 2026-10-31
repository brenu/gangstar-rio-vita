#ifndef GANGSTAR_RENDER_SAMPLE_H
#define GANGSTAR_RENDER_SAMPLE_H
#include <stdint.h>
enum {RIO_RENDER_ARRAYS,RIO_RENDER_ELEMENTS,RIO_RENDER_MATRIX,RIO_RENDER_UNIFORM4,RIO_RENDER_UNIFORM1,RIO_RENDER_LOCATION,RIO_RENDER_SYNC,RIO_RENDER_KINDS};
uint64_t gangstar_render_begin(unsigned,unsigned);
void gangstar_render_end(unsigned,uint64_t);
void gangstar_render_report(void);
static inline int rio_render_choose(uint32_t *state) {
 uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;*state=x;return (x&31)==0;
}
#endif
