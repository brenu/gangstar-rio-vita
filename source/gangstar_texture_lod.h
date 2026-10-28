#ifndef GANGSTAR_TEXTURE_LOD_H
#define GANGSTAR_TEXTURE_LOD_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef struct { uint32_t skip, output_size; unsigned char header[128]; } RioTextureLod;
static inline uint32_t rio_lod_u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline void rio_lod_put(unsigned char *p, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) p[i] = n >> (8 * i);
}
#ifndef GANGSTAR_TEXTURE_LOD_EDGE
#define GANGSTAR_TEXTURE_LOD_EDGE 1024
#endif
#if GANGSTAR_TEXTURE_LOD_EDGE == 512
#define RIO_LOD_SKIP 2621440u
#define RIO_LOD_LEVELS 10
#elif GANGSTAR_TEXTURE_LOD_EDGE == 1024
#define RIO_LOD_SKIP 2097152u
#define RIO_LOD_LEVELS 11
#else
#error Unsupported world texture edge
#endif
#define RIO_LOD_OUTPUT_SIZE (2796344u - RIO_LOD_SKIP)
#ifdef GANGSTAR_WORLD_TEXTURE_LOD
#include "gangstar_world_texture_manifest.h"
#define RIO_LOD_RAM_LIMIT (64 * 1024 * 1024)
#define RIO_LOD_SLOTS 384
#define RIO_LOD_FAMILIES RIO_WORLD_LIGHTMAP_FAMILIES
#else
#define RIO_LOD_RAM_LIMIT (12 * 1024 * 1024)
#define RIO_LOD_SLOTS 32
#define RIO_LOD_FAMILIES 2
#endif
static inline unsigned rio_lod_lightmap_members(unsigned family) {
#ifdef GANGSTAR_WORLD_TEXTURE_LOD
    return family<RIO_WORLD_LIGHTMAP_FAMILIES ? rio_world_lightmaps[family].members : 0;
#else
    return family<2 ? 15 : 0;
#endif
}
// Separate transactional preparation state for each declared lightmap family.
static inline int rio_lod_lightmap_family(const char *name) {
    if (!name || strnlen(name, 256) == 256) return -1;
    const char *base = strrchr(name, '/'); base = base ? base + 1 : name;
#ifdef GANGSTAR_WORLD_TEXTURE_LOD
    for (unsigned i=0;i<RIO_WORLD_LIGHTMAP_FAMILIES;++i) {
        size_t n=strlen(rio_world_lightmaps[i].prefix);
        if (!strncmp(base,rio_world_lightmaps[i].prefix,n) && base[n]>='0' && base[n]<='3' &&
            !strcmp(base+n+1,".tga") && (rio_world_lightmaps[i].members & (1u<<(base[n]-'0')))) return (int)i;
    }
#else
    const char *prefixes[] = {"copacabana_lightmap0", "southfavela_lightmap0"};
    for (unsigned i = 0; i < 2; ++i) {
        size_t n = strlen(prefixes[i]);
        if (!strncmp(base, prefixes[i], n) && base[n] >= '0' && base[n] <= '3' && !strcmp(base + n + 1, ".tga")) return i;
    }
#endif
    return -1;
}
static inline int rio_lod_lightmap_name(const char *name) { return rio_lod_lightmap_family(name) >= 0; }
static inline int rio_lod_name(const char *name) {
    if (!name || strnlen(name, 256) == 256) return 0;
    if (rio_lod_lightmap_name(name)) return 1;
    const char *base = strrchr(name, '/'); base = base ? base + 1 : name;
#ifdef GANGSTAR_WORLD_TEXTURE_LOD
    size_t first=0, count=sizeof(rio_world_texture_names)/sizeof(*rio_world_texture_names);
    while (count) {
        size_t half=count/2, middle=first+half;
        int comparison=strcmp(rio_world_texture_names[middle],base);
        if (comparison<0) { first=middle+1; count-=half+1; } else count=half;
    }
    return first<sizeof(rio_world_texture_names)/sizeof(*rio_world_texture_names) && !strcmp(rio_world_texture_names[first],base);
#else
    return !strcmp(base, "ground_copa_01.tga") ||
        !strcmp(base, "southfavela_merged_atlas_exported_atlas0.tga") ||
        !strcmp(base, "cityofgod_tile.tga") ||
        !strcmp(base, "safehouse_abandonned_building_00_merged_atlas0.tga") ||
        !strcmp(base, "safehouse_abandonned_building_00_merged_atlas0_alpha.tga") ||
        !strcmp(base, "ground_copa_00.tga") ||
        !strcmp(base, "ground_copa_01_spec.tga") ||
        !strcmp(base, "copacabana_merge_atlas_atlas0.tga") ||
        !strcmp(base, "copacabana_merge_atlas_atlas0_alpha.tga") ||
        !strcmp(base, "southfavela_merged_atlas_exported_atlas1.tga") ||
        !strcmp(base, "southfavela_merged_atlas_exported_atlas1_alpha.tga") ||
        !strcmp(base, "southfavela_merged_atlas_exported_atlas0_alpha.tga") ||
        !strcmp(base, "ground_00.tga") ||
        !strcmp(base, "copacabana_merge_atlas_atlas1.tga") ||
        !strcmp(base, "copacabana_merge_atlas_atlas1_alpha.tga") ||
        !strcmp(base, "cliff.tga") ||
        !strcmp(base, "cliff_alpha.tga") ||
        !strcmp(base, "clifftexture02.tga") ||
        !strcmp(base, "tree_generic_lod.tga") ||
        !strcmp(base, "tree_generic_lod_alpha.tga") ||
        !strcmp(base, "copacabana_merge_atlas_alpha_atlas2.tga") ||
        !strcmp(base, "copacabana_merge_atlas_alpha_atlas2_alpha.tga") ||
        !strcmp(base, "southfavela_merged_atlas_exported_alpha_atlas2.tga") ||
        !strcmp(base, "southfavela_merged_atlas_exported_alpha_atlas2_alpha.tga");
#endif
}
// Lightmap layout remains unchanged; reduce other declared world sources.
static inline int rio_lod_replace_source(const char *name) {
    return rio_lod_name(name) && !rio_lod_lightmap_name(name);
}
// Exact supported Rio DDS/ETC0 2D chain only. Retain existing compressed mips;
// no decoding, filtering, or recompression. Unknown layouts use original data.
static inline int rio_lod_plan(const unsigned char *h, size_t size, RioTextureLod *out) {
    if (size < 128) return 0;
    unsigned source_width=rio_lod_u32(h+16), source_height=rio_lod_u32(h+12);
    unsigned source=source_width>source_height?source_width:source_height;
    unsigned levels=source==2048?12:11;
    int shape=(source_width==source_height && (source==1024 || source==2048)) ||
        (source_width==512 && source_height==1024) || (source_width==1024 && source_height==512);
    if (!shape || source<=GANGSTAR_TEXTURE_LOD_EDGE) return 0;
    if (rio_lod_u32(h) != 0x20534444 || rio_lod_u32(h + 4) != 124 ||
        rio_lod_u32(h + 8) != 0x21007 || rio_lod_u32(h + 20) || rio_lod_u32(h + 24) ||
        rio_lod_u32(h + 28) != levels || rio_lod_u32(h + 76) != 32 ||
        rio_lod_u32(h + 80) != 4 || rio_lod_u32(h + 84) != 0x30435445 ||
        rio_lod_u32(h + 108) != 0x401008 || rio_lod_u32(h + 112) ||
        rio_lod_u32(h + 116) || rio_lod_u32(h + 120) || rio_lod_u32(h + 124)) return 0;
    for (unsigned i = 32; i < 76; i += 4) if (rio_lod_u32(h + i)) return 0;
    for (unsigned i = 88; i < 108; i += 4) if (rio_lod_u32(h + i)) return 0;
    uint32_t payload=0,keep=0,width=source_width,height=source_height,skipped_levels=0;
    for (unsigned level = 0; level < levels; ++level) {
        unsigned level_bytes=((width+3)/4)*((height+3)/4)*8;
        payload+=level_bytes;
        if (width<=GANGSTAR_TEXTURE_LOD_EDGE && height<=GANGSTAR_TEXTURE_LOD_EDGE) keep+=level_bytes;
        else ++skipped_levels;
        if(width>1) width>>=1;
        if(height>1) height>>=1;
    }
    if (size != 128 + payload) return 0;
    out->skip = payload - keep;
    out->output_size = size - out->skip;
    memcpy(out->header, h, 128);
    rio_lod_put(out->header+12,source_height>>skipped_levels);
    rio_lod_put(out->header+16,source_width>>skipped_levels);
    rio_lod_put(out->header+28,levels-skipped_levels);
    return 1;
}
// Retain four originals, leaving menu allocation headroom. Uncached
// time-of-day variants still use their original files, without mip rewriting.
static inline int rio_lod_original_source(const char *name) {
    int family=rio_lod_lightmap_family(name);
    if (family<0) return 0;
#ifdef GANGSTAR_WORLD_TEXTURE_LOD
    if (rio_world_lightmaps[family].source_edge!=2048) return 0;
#endif
    const char *base=strrchr(name,'/');base=base?base+1:name;
    static const char *const targets[]={
        "rich_00_lightmap00.tga", "rich_01_lightmap00.tga",
        "sugarloaf_lightmap00.tga", "mountain_lightmap00.tga"
    };
    for (unsigned i=0;i<sizeof(targets)/sizeof(*targets);++i)
        if (!strcmp(base,targets[i])) return 1;
    return 0;
}
static inline int rio_lod_cache_source(const char *name) {
    return rio_lod_replace_source(name) || rio_lod_original_source(name);
}
const void *gangstar_texture_original_get(int, uint64_t, uint32_t, const char *, uint32_t *);
const void *gangstar_texture_lod_get(int, uint64_t, uint32_t, const char *, uint32_t *);
void gangstar_texture_lod_report(unsigned);
#endif
