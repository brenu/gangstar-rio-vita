#ifndef VGL_ALPHA_OVERWRITE_H
#define VGL_ALPHA_OVERWRITE_H
/* Only a complete single-level image has no old bytes to preserve. */
static inline int vgl_alpha_complete_overwrite(int alpha_u8, unsigned bpp,
    unsigned mip_count, int level, int x, int y, int width, int height,
    unsigned original_width, unsigned original_height, int has_pixels) {
    return alpha_u8 && bpp == 1 && mip_count == 1 && level == 0 &&
        x == 0 && y == 0 && width > 0 && height > 0 && has_pixels &&
        (unsigned)width == original_width && (unsigned)height == original_height &&
        original_width % 8 == 0;
}
#endif
