#include "gangstar_texture_lod.h"
#include "gangstar_source_cache.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
static unsigned char header[128];
static unsigned reads, read_bytes;
static int corrupt, fail_tail, interrupt_once, original_mode;
int __wrap_open(const char *name, int flags, ...) { assert(strstr(name, "main.12030") && flags == O_RDONLY); return 99; }
int __wrap_fstat(int fd, struct stat *st) { assert(fd == 99); memset(st, 0, sizeof(*st)); st->st_size = 2119380688; return 0; }
void gangstar_archive_track(int fd, const char *name, int flags) { assert(fd == 99 && strstr(name, ".obb") && flags == O_RDONLY); }
ssize_t gangstar_archive_pread(int fd, void *data, size_t length, off_t offset) {
    assert(fd == 99);
    if (interrupt_once) { interrupt_once = 0; errno = EINTR; return -1; }
    ++reads;
    uint64_t relative = ((uint64_t)offset - 100) % 4000000;
    if (relative == 0) {
        assert(length == 128); memcpy(data, header, 128);
        if (corrupt) ((unsigned char *)data)[84] = 0;
    } else {
        assert(relative >= 128 + (original_mode?0:RIO_LOD_SKIP) && relative + length <= 2796344);
        if (fail_tail) return 0;
        // Simulate legal short positional reads.
        if (length > 8192) length = 8192;
        for (size_t i = 0; i < length; ++i) ((unsigned char *)data)[i] = (relative + i) & 255;
    }
    read_bytes += length; return length;
}
static void *cached_reader(void *unused) {
    (void)unused;
    for(unsigned i=0;i<32;++i) {
        uint32_t size=0;
        const unsigned char *p=gangstar_texture_original_get(0,100,2796344,"rich_00_lightmap00.tga",&size);
        assert(p && size==2796344 && !memcmp(p,header,128) && p[size-1]==((size-1)&255));
        free((void *)p);
    }
    return NULL;
}
int main(int argc, char **argv) {
    (void)argv;
    assert(rio_lod_name("_packEtc/copacabana_lightmap00.tga"));
    assert(!rio_lod_name("hud.tga") && !rio_lod_name("character.tga") && !rio_lod_name("x_lightmap.tga"));
    assert(!rio_lod_name("x_lightmap0.tga") && !rio_lod_name("x_lightmap00.tga.extra"));
    assert(rio_lod_name("_packEtc/ground_copa_01.tga"));
    assert(rio_lod_name("_packEtc/southfavela_merged_atlas_exported_atlas0.tga"));
    assert(!rio_lod_lightmap_name("ground_copa_01.tga"));
#ifndef GANGSTAR_WORLD_TEXTURE_LOD
    assert(!rio_lod_name("ground_copa_02.tga") && !rio_lod_name("southfavela_merged_atlas_exported_atlas2.tga"));
#endif
    const char *batch[] = {"cityofgod_tile.tga", "safehouse_abandonned_building_00_merged_atlas0.tga",
        "safehouse_abandonned_building_00_merged_atlas0_alpha.tga", "ground_copa_00.tga", "ground_copa_01_spec.tga", "copacabana_merge_atlas_atlas0.tga", "copacabana_merge_atlas_atlas0_alpha.tga",
        "southfavela_merged_atlas_exported_atlas1.tga", "southfavela_merged_atlas_exported_atlas1_alpha.tga",
        "southfavela_merged_atlas_exported_atlas0_alpha.tga", "ground_00.tga"};
    for (unsigned i = 0; i < sizeof(batch)/sizeof(*batch); ++i) assert(rio_lod_name(batch[i]));
    for (unsigned member = 0; member < 4; ++member) {
        char south[] = "_packEtc/southfavela_lightmap00.tga";
        south[strlen(south) - 5] = '0' + member;
        assert(rio_lod_name(south) && rio_lod_lightmap_family(south) == 1);
    }
    assert(rio_lod_lightmap_family("copacabana_lightmap03.tga") == 0);
    assert(rio_lod_lightmap_family("southfavela_lightmap04.tga") == -1);
#ifdef GANGSTAR_WORLD_TEXTURE_LOD
    assert(rio_lod_lightmap_family("northfavela_lightmap00.tga")>=0);
    assert(rio_lod_lightmap_members(rio_lod_lightmap_family("interior_nightclub_lightmap00.tga"))==1);
    assert(rio_lod_lightmap_members(rio_lod_lightmap_family("interior_bank_lightmap00.tga"))==3);
    assert(rio_lod_lightmap_family("interior_bank_lightmap02.tga")==-1);
    for (unsigned i=0;i<sizeof(rio_world_texture_names)/sizeof(*rio_world_texture_names);++i) {
        assert(rio_lod_name(rio_world_texture_names[i]));
        if(i)assert(strcmp(rio_world_texture_names[i-1],rio_world_texture_names[i])<0);
    }
    for(unsigned family=0;family<RIO_WORLD_LIGHTMAP_FAMILIES;++family)
        for(unsigned member=0;member<4;++member) if(rio_lod_lightmap_members(family)&(1u<<member)) {
            char map[192];snprintf(map,sizeof(map),"%s%u.tga",rio_world_lightmaps[family].prefix,member);
            assert(rio_lod_name(map) && rio_lod_lightmap_family(map)==(int)family);
        }
    assert(!rio_lod_name("char_maincharacter.tga") && !rio_lod_name("huds.bmp"));
    assert(!rio_lod_name("car_cops_texture_d.tga"));
#else
    assert(rio_lod_lightmap_family("northfavela_lightmap00.tga") == -1);
#endif
    unsigned offsets[] = {0,4,8,12,16,28,76,80,84,108};
    uint32_t values[] = {0x20534444,124,0x21007,2048,2048,12,32,4,0x30435445,0x401008};
    for (unsigned i = 0; i < sizeof(offsets)/sizeof(*offsets); ++i) rio_lod_put(header + offsets[i], values[i]);
    // New-game and Continue must see the same original lightmap layout.
    assert(!rio_lod_replace_source(NULL));
    assert(!rio_lod_replace_source("copacabana_lightmap00.tga"));
    assert(!rio_lod_replace_source("_packetc/southfavela_lightmap03.tga"));
    assert(rio_lod_replace_source("_packetc/copacabana_merge_atlas_atlas0.tga"));
#ifdef GANGSTAR_WORLD_TEXTURE_LOD
    for (unsigned i=0;i<RIO_WORLD_LIGHTMAP_FAMILIES;++i)
        for (unsigned m=0;m<4;++m) if (rio_lod_lightmap_members(i)&(1u<<m)) {
            char name[192]; snprintf(name,sizeof(name),"%s%u.tga",rio_world_lightmaps[i].prefix,m);
            assert(!rio_lod_replace_source(name));
        }
#endif
#ifdef GANGSTAR_WORLD_TEXTURE_LOD
    uint64_t source_budget=0; unsigned originals=0,preloaded=0;
    for (unsigned i=0;i<sizeof(rio_world_texture_names)/sizeof(*rio_world_texture_names);++i) {
        const char *name=rio_world_texture_names[i];
        if (!rio_lod_preload_source(name)) continue;
        ++preloaded;
        if (rio_lod_original_source(name)) { source_budget+=2796344; ++originals; }
        else if (rio_lod_replace_source(name)) source_budget+=rio_world_texture_bytes[i];
    }
    assert(originals==4 && preloaded==243 && source_budget==52725304);
    assert(source_budget < 64u*1024u*1024u);
    assert(sizeof(rio_lod_supplemental_sources)/sizeof(*rio_lod_supplemental_sources)==4);
    assert(!strcmp(rio_lod_supplemental_sources[3],"airport_lightmap00.tga"));
    for(unsigned i=0;i<4;++i) {
        const char *name=rio_lod_supplemental_sources[i];
        assert(rio_lod_original_source(name) && !rio_lod_preload_source(name));
        unsigned matches=0;
        for(unsigned j=0;j<sizeof(rio_world_texture_names)/sizeof(*rio_world_texture_names);++j)
            matches+=!strcmp(name,rio_world_texture_names[j]);
        assert(matches==1);
    }
    assert(source_budget+4ull*2796344 < RIO_SOURCE_CACHE_LIMIT);
    assert(!rio_lod_preload_source("copacabana_lightmap02.tga"));
    const char *route_maps[]={"rich_00_lightmap00.tga","rich_01_lightmap00.tga",
        "sugarloaf_lightmap00.tga","mountain_lightmap00.tga"};
    for(unsigned i=0;i<sizeof(route_maps)/sizeof(*route_maps);++i)
        assert(rio_lod_original_source(route_maps[i]) && !rio_lod_replace_source(route_maps[i]));
    assert(rio_lod_original_source("rich_00_lightmap01.tga"));
    assert(rio_lod_original_source("copacabana_lightmap02.tga"));
    assert(rio_lod_original_source("southfavela_lightmap03.tga"));
    assert(rio_lod_original_source("_packetc/copacabana_lightmap01.tga"));
    assert(!rio_lod_original_source("mountain_lightmap00.tga.extra"));
#endif
    if (argc>1) {
        original_mode=1;uint32_t length=0;interrupt_once=1;
        // A gameplay miss reads once into an independently owned cache entry.

        const unsigned char *raw=gangstar_texture_original_get(0,100,2796344,"rich_00_lightmap00.tga",&length);
        assert(raw && length==2796344 && !memcmp(raw,header,128));
        for(unsigned i=128;i<length;++i) assert(raw[i]==(i&255));
        gangstar_texture_source_preloading(0);
        unsigned before=reads;
        const void *again=gangstar_texture_original_get(0,100,2796344,"rich_00_lightmap00.tga",&length);
        assert(again && again!=raw && reads==before && !memcmp(again,raw,length));
        free((void *)again); free((void *)raw);
        assert(!gangstar_texture_original_get(0,100,2796344,"rich_00_lightmap04.tga",&length));
        assert(!gangstar_texture_original_get(0,100,2796344,"huds.bmp",&length));
        gangstar_texture_source_preloading(1);
        fail_tail=1;assert(!gangstar_texture_original_get(0,4000100,2796344,"sugarloaf_lightmap00.tga",&length));
        fail_tail=0;const void *sugar=gangstar_texture_original_get(0,4000100,2796344,"sugarloaf_lightmap00.tga",&length);
        assert(sugar); free((void *)sugar);
        corrupt=1;assert(!gangstar_texture_original_get(0,8000100,2796344,"mountain_lightmap00.tga",&length));
        corrupt=0;
        const unsigned char *route=gangstar_texture_original_get(0,8000100,2796344,"rich_00_lightmap00.tga",&length);
        assert(route && length==2796344 && !memcmp(route,header,128));
        before=reads;
        again=gangstar_texture_original_get(0,8000100,2796344,"rich_00_lightmap00.tga",&length);
        assert(again && again!=route && reads==before && !memcmp(again,route,length));
        free((void *)again); free((void *)route);
        gangstar_texture_source_preloading(0);
        before=reads;
        const void *demand=gangstar_texture_original_get(0,12000100,2796344,"copacabana_lightmap00.tga",&length);
        assert(demand && reads>before);free((void *)demand);
        RioTextureSourcePriority wanted={12000100,2796344,1,4};
        gangstar_texture_source_priorities(&wanted,1);
        assert(gangstar_texture_source_background(12000100,2796344,"copacabana_lightmap00.tga"));
        before=reads;
        const void *prefetched=gangstar_texture_original_get(0,12000100,2796344,"copacabana_lightmap00.tga",&length);
        assert(prefetched && reads==before);free((void *)prefetched);
        pthread_t readers[2];
        for(unsigned i=0;i<2;++i)assert(!pthread_create(&readers[i],NULL,cached_reader,NULL));
        for(unsigned i=0;i<32;++i)gangstar_texture_source_priorities(&wanted,1);
        for(unsigned i=0;i<2;++i)assert(!pthread_join(readers[i],NULL));
        assert(reads==before);
        // Fill spare space, then confirm misses do no I/O and old assets survive.
        const unsigned full_sources=RIO_SOURCE_CACHE_LIMIT/2796344u;
        for(unsigned i=4;i<full_sources;++i) {
            void *fill=(void *)gangstar_texture_original_get(0,100+4000000ull*i,2796344,"rich_00_lightmap00.tga",&length);
            assert(fill);free(fill);
        }
        before=reads;
        assert(!gangstar_texture_original_get(0,100+4000000ull*full_sources,2796344,"rich_00_lightmap00.tga",&length));
        assert(reads==before);
        void *retained=(void *)gangstar_texture_original_get(0,100,2796344,"rich_00_lightmap00.tga",&length);
        assert(retained && reads==before);free(retained);
        // Actual positional range helper: short reads/EINTR, bounds and failure.
        unsigned char tail[10936];interrupt_once=1;
        assert(gangstar_lightmap_read_range(100,tail,2785408u,sizeof(tail)));
        for(unsigned i=0;i<sizeof(tail);++i)assert(tail[i]==((2785408u+i)&255));
        before=reads;
        assert(!gangstar_lightmap_read_range(100,tail,2796344u,1));
        assert(!gangstar_lightmap_read_range(2119380688u,tail,0,1));
        assert(!gangstar_lightmap_read_range(100,NULL,0,1));
        assert(reads==before);
        fail_tail=1;assert(!gangstar_lightmap_read_range(100,tail,2785408u,sizeof(tail)));fail_tail=0;
        puts("Original lightmap readers: byte identity, independent ownership, exact families, interrupted/short reads and failed-fill fallback passed");
        return 0;
    }
    RioTextureLod plan;
    assert(rio_lod_plan(header, 2796344, &plan));
    assert(plan.output_size == RIO_LOD_OUTPUT_SIZE && plan.skip == RIO_LOD_SKIP);
    assert(rio_lod_u32(plan.header + 12) == GANGSTAR_TEXTURE_LOD_EDGE && rio_lod_u32(plan.header + 16) == GANGSTAR_TEXTURE_LOD_EDGE && rio_lod_u32(plan.header + 28) == RIO_LOD_LEVELS);
    assert(!rio_lod_plan(header, 2796343, &plan) && !rio_lod_plan(header, 2796345, &plan));
    for (unsigned i = 0; i < 128; ++i) {
        unsigned char saved = header[i]; header[i] ^= 128;
        assert(!rio_lod_plan(header, 2796344, &plan)); header[i] = saved;
    }
    unsigned char small_header[128]; memcpy(small_header, header, 128);
    rio_lod_put(small_header + 12,1024);rio_lod_put(small_header + 16,1024);rio_lod_put(small_header + 28,11);
#if GANGSTAR_TEXTURE_LOD_EDGE == 512
    assert(rio_lod_plan(small_header,699192,&plan) && plan.output_size==174904 && plan.skip==524288);
#else
    assert(!rio_lod_plan(small_header,699192,&plan));
#endif
#if GANGSTAR_TEXTURE_LOD_EDGE == 512
    unsigned char rectangle[128];memcpy(rectangle,small_header,128);
    rio_lod_put(rectangle+16,512);
    assert(rio_lod_plan(rectangle,349672,&plan));
    assert(plan.output_size==87528 && plan.skip==262144 && rio_lod_u32(plan.header+16)==256 && rio_lod_u32(plan.header+12)==512 && rio_lod_u32(plan.header+28)==10);
    rio_lod_put(rectangle+16,1024);rio_lod_put(rectangle+12,512);
    assert(rio_lod_plan(rectangle,349672,&plan) && rio_lod_u32(plan.header+16)==512 && rio_lod_u32(plan.header+12)==256);
    assert(!rio_lod_plan(rectangle,349671,&plan));
#endif
    uint32_t size = 0;
    interrupt_once = 1;
    const unsigned char *first = gangstar_texture_lod_get(0,100,2796344,"copacabana_lightmap00.tga",&size);
    assert(first && size == RIO_LOD_OUTPUT_SIZE && read_bytes == size);
    assert(rio_lod_u32(first + 16) == GANGSTAR_TEXTURE_LOD_EDGE);
    for (unsigned i = 128; i < size; ++i) assert(first[i] == ((i + RIO_LOD_SKIP) & 255));
    unsigned before = reads;
    const void *second = gangstar_texture_lod_get(0,100,2796344,"copacabana_lightmap00.tga",&size);
    assert(second && second != first && reads == before && !memcmp(first, second, size));
    free((void *)first); free((void *)second);
    before = reads;
    assert(!gangstar_texture_lod_get(0,100,2796344,"hud.tga",&size) && reads == before);
    corrupt = 1; assert(!gangstar_texture_lod_get(0,120000100,2796344,"copacabana_lightmap00.tga",&size)); corrupt = 0;
    fail_tail = 1; assert(!gangstar_texture_lod_get(0,120000100,2796344,"copacabana_lightmap00.tga",&size)); fail_tail = 0;
    // Streaming past the old lifetime cache limit must keep returning reduced
    // data; the reader destroys each buffer rather than retaining all regions.
    for (unsigned i = 0; i < 400; ++i) {
        const void *owned = gangstar_texture_lod_get(0,100+4000000u*i,2796344,"copacabana_lightmap00.tga",&size);
        assert(owned && size == RIO_LOD_OUTPUT_SIZE);
        free((void *)owned);
    }
    assert(!gangstar_texture_lod_get(-1,100,2796344,"copacabana_lightmap00.tga",&size));
    assert(!gangstar_texture_lod_get(0,2119380680,2796344,"copacabana_lightmap00.tga",&size));
    puts("Texture LOD: fresh original-data reads, exact lower mips, skipped top level, short reads/EINTR, corrupt/truncated fallback, independent owned buffers and streaming beyond former cache limits passed");
}
