#ifndef GANGSTAR_LIGHTMAP_POOL_H
#define GANGSTAR_LIGHTMAP_POOL_H
#include <stddef.h>
/* Buffers for whole 2048x2048 ETC1 chains (2796216 bytes mapped, 2796344 as a
 * DDS), reserved while the heap is still contiguous. A long session fragments
 * the heap until no such block is left, and ITexture::map (ELF 75e784) does not
 * survive a failed one: the lightmap is drawn pink. Any thread may call these. */
#define RIO_LIGHTMAP_SLOTS 3u
#define RIO_LIGHTMAP_SLOT_BYTES 2796352u
#define RIO_LIGHTMAP_MIN_BYTES 2796200u
void gangstar_lightmap_pool_init(void);
void *gangstar_lightmap_pool_get(size_t size);   // NULL: other size, full or not reserved
int gangstar_lightmap_pool_put(void *ptr);       // 1 when ptr belonged to the pool
int gangstar_lightmap_pool_owns(const void *ptr);
void gangstar_lightmap_pool_report(void);
#endif
