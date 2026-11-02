#ifndef GANGSTAR_TEXTURE_FAULT_H
#define GANGSTAR_TEXTURE_FAULT_H
#include <stdint.h>
#include <string.h>
// Diagnostic only: fail the tail of one large lightmap upload, not unrelated
// textures or later uploads. No native objects are retained by this state.
typedef struct { unsigned armed; uintptr_t selected; } RioTextureFault;
static inline int rio_texture_fault(RioTextureFault *s, const char *name,
    uintptr_t texture, int level, unsigned base_w, unsigned base_h) {
    if (!name || !strstr(name,"_lightmap") || !texture ||
        base_w != 2048 || base_h != 2048 || level < 5 || level > 11) return 0;
    if (s->selected) return s->selected == texture;
    if (!s->armed) return 0;
    s->armed=0; s->selected=texture; return 1;
}
#endif
