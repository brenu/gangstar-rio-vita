#ifndef GANGSTAR_RESIDENT_POLICY_H
#define GANGSTAR_RESIDENT_POLICY_H
#include <stdint.h>
#include <string.h>
#define RIO_RESIDENT_LIMIT (8u * 1024 * 1024)
#define RIO_RESIDENT_RESERVE (8u * 1024 * 1024)
typedef struct { uintptr_t manager, texture; unsigned bytes; } RioResidentEntry;
typedef struct { RioResidentEntry entries[8]; unsigned count, bytes; } RioResidentPolicy;
static inline int rio_resident_name(const char *name) {
    if (!name) return 0;
    const char *p = strrchr(name, '/'); p = p ? p + 1 : name;
    const char *names[] = {"copacabana_merge_atlas_atlas0.tga", "copacabana_merge_atlas_atlas0_alpha.tga",
        "southfavela_merged_atlas_exported_atlas0.tga", "southfavela_merged_atlas_exported_atlas0_alpha.tga",
        "southfavela_merged_atlas_exported_atlas1.tga", "southfavela_merged_atlas_exported_atlas1_alpha.tga",
        "southfavela_lightmap00.tga"};
    for (unsigned i = 0; i < sizeof(names)/sizeof(*names); ++i) if (!strcmp(p, names[i])) return 1;
    return 0;
}
// Budget accounting only: no ownership, refs or native object dereferences.
// Entries are never reclaimed, conservatively charging explicitly removed objects.
static inline int rio_resident_keep(RioResidentPolicy *p, uintptr_t manager, uintptr_t texture,
    const char *name, unsigned width, unsigned height, int ready, int bound, uint64_t free_vram) {
    if (!manager || !texture || !ready || !bound || !rio_resident_name(name) ||
        width != height || (width != 1024 && width != 2048) || free_vram < RIO_RESIDENT_RESERVE) return 0;
    for (unsigned i = 0; i < p->count; ++i)
        if (p->entries[i].manager == manager && p->entries[i].texture == texture) return 1;
    unsigned cost = 0;
    for (unsigned w = width; ; w >>= 1) {
        unsigned blocks = (w + 3) / 4; cost += blocks * blocks * 8;
        if (w == 1) break;
    }
    if (p->count == 8 || cost > RIO_RESIDENT_LIMIT - p->bytes) return 0;
    p->entries[p->count++] = (RioResidentEntry){manager, texture, cost}; p->bytes += cost;
    return 1;
}
#endif
