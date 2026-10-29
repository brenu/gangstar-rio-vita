#ifndef VGL_COMPRESSED_MIP_LAYOUT_H
#define VGL_COMPRESSED_MIP_LAYOUT_H
#include <stdint.h>
/* A failed base replacement can leave a smaller, still usable texture. Never
 * size its allocation using that descriptor and write a different mip layout. */
static inline int vgl_compressed_mip_matches(int level, uint32_t base_w,
    uint32_t base_h, uint32_t w, uint32_t h) {
    if (level < 0 || !base_w || !base_h || !w || !h) return 0;
    while (level--) {
        if (base_w == 1 && base_h == 1) return 0;
        if (base_w > 1) base_w >>= 1;
        if (base_h > 1) base_h >>= 1;
    }
    return w == base_w && h == base_h;
}
#endif
