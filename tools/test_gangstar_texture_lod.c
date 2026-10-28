#include "gangstar_texture_lod.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
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
    uint64_t source_budget=0; unsigned originals=0;
    for (unsigned i=0;i<sizeof(rio_world_texture_names)/sizeof(*rio_world_texture_names);++i) {
        const char *name=rio_world_texture_names[i];
        if (rio_lod_original_source(name)) { source_budget+=2796344; ++originals; }
        else if (rio_lod_replace_source(name)) source_budget+=rio_world_texture_bytes[i];
    }
    assert(originals==4 && source_budget==52725304 && source_budget<=RIO_LOD_RAM_LIMIT);
    const char *route_maps[]={"rich_00_lightmap00.tga","rich_01_lightmap00.tga",
        "sugarloaf_lightmap00.tga","mountain_lightmap00.tga"};
    for(unsigned i=0;i<sizeof(route_maps)/sizeof(*route_maps);++i)
        assert(rio_lod_original_source(route_maps[i]) && !rio_lod_replace_source(route_maps[i]));
    assert(!rio_lod_original_source("rich_00_lightmap01.tga"));
    assert(!rio_lod_original_source("copacabana_lightmap02.tga"));
    assert(!rio_lod_original_source("southfavela_lightmap03.tga"));
    assert(!rio_lod_original_source("_packetc/copacabana_lightmap01.tga"));
    assert(!rio_lod_original_source("mountain_lightmap00.tga.extra"));
#endif
    if (argc>1) {
        original_mode=1;uint32_t length=0;interrupt_once=1;
        const unsigned char *raw=gangstar_texture_original_get(0,100,2796344,"rich_00_lightmap00.tga",&length);
        assert(raw && length==2796344 && !memcmp(raw,header,128));
        for(unsigned i=128;i<length;++i) assert(raw[i]==(i&255));
        unsigned before=reads;
        assert(gangstar_texture_original_get(0,100,2796344,"rich_00_lightmap00.tga",&length)==raw && reads==before);
        assert(!gangstar_texture_original_get(0,100,2796344,"rich_00_lightmap01.tga",&length));
        assert(!gangstar_texture_original_get(0,100,2796344,"huds.bmp",&length));
        fail_tail=1;assert(!gangstar_texture_original_get(0,4000100,2796344,"sugarloaf_lightmap00.tga",&length));
        fail_tail=0;assert(gangstar_texture_original_get(0,4000100,2796344,"sugarloaf_lightmap00.tga",&length));
        corrupt=1;assert(!gangstar_texture_original_get(0,8000100,2796344,"mountain_lightmap00.tga",&length));
        corrupt=0;
        const unsigned char *route=gangstar_texture_original_get(0,8000100,2796344,"rich_00_lightmap00.tga",&length);
        assert(route && length==2796344 && !memcmp(route,header,128));
        before=reads;
        assert(gangstar_texture_original_get(0,8000100,2796344,"rich_00_lightmap00.tga",&length)==route && reads==before);
        puts("Original lightmap cache: full byte identity, retained hits, exact families, interrupted/short reads and failed-fill fallback passed");
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
    assert(gangstar_texture_lod_get(0,100,2796344,"copacabana_lightmap00.tga",&size) == first && reads == before);
    assert(gangstar_texture_lod_get(0,100+4000000,2796344,"southfavela_lightmap00.tga",&size));
    unsigned after_south = reads;
    assert(gangstar_texture_lod_get(0,100+4000000,2796344,"southfavela_lightmap00.tga",&size) && reads == after_south);
    // This retained South Favela allocation also counts toward capacity.
    before = reads;
    assert(!gangstar_texture_lod_get(0,100,2796344,"hud.tga",&size) && reads == before);
    corrupt = 1; assert(!gangstar_texture_lod_get(0,100+4000000*30,2796344,"copacabana_lightmap00.tga",&size)); corrupt = 0;
    fail_tail = 1; assert(!gangstar_texture_lod_get(0,100+4000000*30,2796344,"copacabana_lightmap00.tga",&size)); fail_tail = 0;
    unsigned capacity = RIO_LOD_RAM_LIMIT / RIO_LOD_OUTPUT_SIZE;
    if (capacity > RIO_LOD_SLOTS) capacity = RIO_LOD_SLOTS;
    for (unsigned i = 2; i < capacity; ++i) assert(gangstar_texture_lod_get(0,100+4000000*i,2796344,"copacabana_lightmap00.tga",&size));
    before = reads;
    assert(!gangstar_texture_lod_get(0,100+4000000*capacity,2796344,"copacabana_lightmap00.tga",&size) && reads == before + (capacity < RIO_LOD_SLOTS ? 1 : 0));
    before = reads;
    assert(gangstar_texture_lod_get(0,100,2796344,"copacabana_lightmap00.tga",&size) == first && reads == before);
    assert(!gangstar_texture_lod_get(-1,100,2796344,"copacabana_lightmap00.tga",&size));
    assert(!gangstar_texture_lod_get(0,2119380680,2796344,"copacabana_lightmap00.tga",&size));
    puts("Texture LOD: fresh original-data reads, exact lower mips, skipped top level, short reads/EINTR, corrupt/truncated fallback, retained lifetime and configured RAM/slot bounds passed");
}
