#ifndef GANGSTAR_LIGHTMAP_PACK_H
#define GANGSTAR_LIGHTMAP_PACK_H
#include <stdint.h>
#ifdef GANGSTAR_LIGHTMAP_PACK
int gangstar_lightmap_pack_read(uint32_t offset,uint32_t size,void *destination);
#else
static inline int gangstar_lightmap_pack_read(uint32_t offset,uint32_t size,void *destination) {
 (void)offset;(void)size;(void)destination;return 0;
}
#endif
#endif
