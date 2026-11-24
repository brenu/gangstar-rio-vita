#ifndef GANGSTAR_SPILL_POOL_H
#define GANGSTAR_SPILL_POOL_H
#include <stddef.h>
/* Blocks for texture data that no pool, nor the fragmented heap, can hold.
 * Reserved at startup and handed out by vitaGL only after everything else
 * failed; each holds a whole 2048x2048 ETC1 chain, so later mips fit in place.
 * The newlib heap is mapped for the GPU. Any thread may call these. */
#define RIO_SPILL_SLOTS 2u
#define RIO_SPILL_SLOT_BYTES 2797568u // 2796216 rounded up to whole pages
#define RIO_SPILL_MIN_BYTES (1024u * 1024u)
void gangstar_spill_init(void);
void *gangstar_spill_get(size_t alignment, size_t size);   // NULL: unsuitable, full or not reserved
int gangstar_spill_put(void *ptr);                         // 1 when ptr belonged to the pool
size_t gangstar_spill_usable(const void *ptr);             // 0 when it did not
#endif
