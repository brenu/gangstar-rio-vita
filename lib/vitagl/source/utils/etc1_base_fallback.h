#ifndef VGL_ETC1_BASE_FALLBACK_H
#define VGL_ETC1_BASE_FALLBACK_H
#include <stdint.h>
#include "compressed_mip_layout.h"
/* When the base of a large ETC1 chain has no memory, its first level that does
 * fit becomes the base. Texture coordinates are normalized, so it only gets
 * blurrier. The caller reports OOM when not even a small level fits. */
typedef struct {
    uint16_t full_w, full_h; // Base dimensions as the application sees them
    uint8_t pending; // Base dropped, no replacement level uploaded yet
    uint8_t skip; // Levels dropped in front of the stored base
} vgl_etc1_base;
enum { VGL_ETC1_REJECT = -1, VGL_ETC1_IGNORE = 0, VGL_ETC1_UPLOAD = 1 };
// Maps an application level to a stored one; level 0 starts a fresh chain.
static inline int vgl_etc1_base_map(vgl_etc1_base *b, int32_t *level, uint32_t w, uint32_t h) {
    if (*level <= 0) {
        b->pending = b->skip = 0;
        return VGL_ETC1_UPLOAD;
    }
    if (!b->pending && !b->skip) return VGL_ETC1_UPLOAD;
    if (!vgl_compressed_mip_matches(*level, b->full_w, b->full_h, w, h)) return VGL_ETC1_REJECT;
    if (b->pending) {
        b->skip = (uint8_t)*level;
        *level = 0;
        return VGL_ETC1_UPLOAD;
    }
    if (*level < b->skip) return VGL_ETC1_IGNORE;
    *level -= b->skip;
    return VGL_ETC1_UPLOAD;
}
// Called when a stored base has no memory. Returns 1 to wait for a smaller level.
static inline int vgl_etc1_base_drop(vgl_etc1_base *b, uint32_t w, uint32_t h) {
    if (!b->pending) {
        if (w < 512 || h < 512 || w > 4096 || h > 4096) return 0;
        b->full_w = (uint16_t)w;
        b->full_h = (uint16_t)h;
    } else if (w <= 32 || h <= 32) {
        b->pending = b->skip = 0;
        return 0;
    }
    b->pending = 1;
    return 1;
}
#endif
