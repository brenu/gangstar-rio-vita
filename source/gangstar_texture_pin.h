#ifndef GANGSTAR_TEXTURE_PIN_H
#define GANGSTAR_TEXTURE_PIN_H
#include "gangstar_resident_policy.h"
#define RIO_TEXTURE_PIN_LIMIT (3u * 1024 * 1024)
typedef struct { uintptr_t object; unsigned bytes; char name[96]; } RioTexturePin;
typedef struct { RioTexturePin slots[24]; unsigned count, bytes; } RioTexturePins;
static const char *const rio_texture_pin_names[] = {
    "copacabana_merge_atlas_atlas0.tga",
    "copacabana_merge_atlas_atlas0_alpha.tga",
    "southfavela_merged_atlas_exported_atlas0.tga",
    "southfavela_merged_atlas_exported_atlas0_alpha.tga",
    "southfavela_merged_atlas_exported_atlas1.tga",
    "southfavela_merged_atlas_exported_atlas1_alpha.tga",
    "copacabana_merge_atlas_reflect0.tga",
    "copacabana_merge_atlas_reflect0_alpha.tga",
    "southfavela_merged_atlas_exported_reflect0.tga",
    "southfavela_merged_atlas_exported_reflect0_alpha.tga",
    "southfavela_merged_atlas_exported_reflect1.tga",
    "southfavela_merged_atlas_exported_reflect1_alpha.tga",
    "copacabana_merge_atlas_alpha_atlas2.tga",
    "copacabana_merge_atlas_alpha_atlas2_alpha.tga",
    "southfavela_merged_atlas_exported_alpha_atlas2.tga",
    "southfavela_merged_atlas_exported_alpha_atlas2_alpha.tga"
};
#define RIO_TEXTURE_PIN_TARGETS (sizeof(rio_texture_pin_names)/sizeof(rio_texture_pin_names[0]))
static inline int rio_texture_pin_name(const char *name) {
    if (!name) return 0;
    const char *p = strrchr(name, '/'); p = p ? p + 1 : name;
    for (unsigned i = 0; i < RIO_TEXTURE_PIN_TARGETS; ++i) if (!strcmp(p, rio_texture_pin_names[i])) return 1;
    return 0;
}
// Alias only the observed ./ prefix, canonical archive namespace and bare filename. A matching
// basename in another directory can refer to a different asset.
static inline const char *rio_texture_pin_query(const char *name) {
    if (!name) return NULL;
    if (!strncmp(name, "./", 2)) name += 2;
    else if (!strncmp(name, "_packetc/", 9)) name += 9;
    if (strchr(name, '/') || strchr(name, '\\')) return NULL;
    return rio_texture_pin_name(name) ? name : NULL;
}
static inline int rio_texture_pin_key_matches(const char *name, const char *hash) {
    const char *base = rio_texture_pin_query(name);
    const char *key = hash ? rio_texture_pin_query(hash) : base;
    return base && key && !strcmp(base, key);
}
static inline uintptr_t rio_texture_pin_find(const RioTexturePins *p, const char *name) {
    const char *base = rio_texture_pin_query(name);
    if (!base) return 0;
    for (unsigned i = 0; i < p->count; ++i)
        if (!strcmp(p->slots[i].name, base)) return p->slots[i].object;
    return 0;
}
// 1=new retained ref, 2=already retained, 0=ineligible/another instance/budget.
static inline int rio_texture_pin_add(RioTexturePins *p, uintptr_t object, const char *name,
    unsigned width, unsigned height, unsigned file_bytes, unsigned limit,
    void (*add_ref)(void *, uintptr_t), void *context) {
    if (!object || !add_ref || !rio_texture_pin_name(name) || width != 512 || height != 512 || file_bytes != 174904) return 0;
    const char *base = strrchr(name, '/'); base = base ? base + 1 : name;
    if (strlen(base) >= sizeof(p->slots[0].name)) return 0;
    for (unsigned i = 0; i < p->count; ++i) {
        if (p->slots[i].object == object) return 2;
        if (!strcmp(p->slots[i].name, base)) return 0;
    }
    unsigned cost = file_bytes - 128;
    if (p->count == 24 || p->bytes > limit || cost > limit - p->bytes) return 0;
    add_ref(context, object);
    RioTexturePin *slot = &p->slots[p->count++];
    slot->object = object; slot->bytes = cost; strcpy(slot->name, base);
    p->bytes += cost; return 1;
}
// Remove bookkeeping BEFORE native release, so reentrant cleanup cannot double-release.
static inline uintptr_t rio_texture_pin_pop(RioTexturePins *p) {
    if (!p->count) return 0;
    RioTexturePin *slot = &p->slots[--p->count];
    uintptr_t object = slot->object; p->bytes -= slot->bytes;
    *slot = (RioTexturePin){0}; return object;
}
#endif
