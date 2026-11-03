#include "gangstar_texture_lod.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
static unsigned char header[128];
static unsigned reads,read_bytes,minimum_skip=2097152;
static int fail_tail,interrupt_once;
int __wrap_open(const char *name,int flags,...) {assert(strstr(name,"main.12030") && flags==O_RDONLY);return 99;}
int __wrap_fstat(int fd,struct stat *st) {assert(fd==99);memset(st,0,sizeof(*st));st->st_size=2119380688;return 0;}
void gangstar_archive_track(int fd,const char *name,int flags) {(void)flags;assert(fd==99 && strstr(name,".obb"));}
ssize_t gangstar_archive_pread(int fd,void *dst,size_t length,off_t offset) {
    assert(fd==99);
    if(interrupt_once){interrupt_once=0;errno=EINTR;return -1;}
    uint64_t relative=((uint64_t)offset-100)%4000000;
    ++reads;
    if(!relative){assert(length==128);memcpy(dst,header,128);}
    else {
        assert(relative>=128+minimum_skip && relative+length<=2796344);
        if(fail_tail)return 0;
        if(length>8192)length=8192;
        for(size_t i=0;i<length;++i)((unsigned char *)dst)[i]=(relative+i)&255;
    }
    read_bytes+=length;return (ssize_t)length;
}
void _log_print(int type,const char *format,...) {(void)type;(void)format;}
// Include the production reader to check that its fixed cache is untouched.
#include "gangstar_texture_lod.c"
int main(void) {
    const char *names[]={"car_cops_texture_d.tga","car_cops_texture_d_alpha.tga",
        "car_cops_texture_damage_d.tga","car_cops_texture_damage_d_alpha.tga",
        "car_poor_texture_d.tga","car_poor_texture_d_alpha.tga",
        "car_poor_texture_damage_d.tga","car_poor_texture_damage_d_alpha.tga"};
    unsigned offsets[]={0,4,8,12,16,28,76,80,84,108};
    uint32_t values[]={0x20534444,124,0x21007,2048,2048,12,32,4,0x30435445,0x401008};
    for(unsigned i=0;i<10;++i)rio_lod_put(header+offsets[i],values[i]);
    RioTextureLod plan;
    assert(!rio_lod_plan_edge(header,2796344,&plan,0));
    assert(!rio_lod_car_name("copacabana_lightmap00.tga"));
    assert(!rio_lod_car_name("char_maincharacter.tga"));
    assert(!rio_lod_car_name("other/car_cops_texture_d.tga"));
    assert(!rio_lod_car_name("car_update_texture_d.tga"));
    assert(!rio_lod_car_name("car_cops_texture_d.tga.extra"));
    assert(!rio_lod_car_name(NULL));
    for(unsigned i=0;i<8;++i) {
        uint32_t size=0;
#ifdef GANGSTAR_CAR_TEXTURE_LOD
        char canonical[128];snprintf(canonical,sizeof(canonical),"_packEtc/%s",names[i]);
        assert(rio_lod_car_name(names[i]) && rio_lod_car_name(canonical));
        char alias[128];snprintf(alias,sizeof(alias),"./%s",names[i]);assert(rio_lod_car_name(alias));
        snprintf(alias,sizeof(alias),"_packetc/%s",names[i]);assert(rio_lod_car_name(alias));
        assert(!rio_lod_preload_source(names[i]));
        assert(rio_lod_plan_named(header,2796344,&plan,names[i]));
        assert(plan.skip==2097152 && plan.output_size==699192);
        assert(rio_lod_u32(plan.header+12)==1024 && rio_lod_u32(plan.header+16)==1024 && rio_lod_u32(plan.header+28)==11);
        RioSourceKey fixed={0,0,42,8};
        assert(rio_source_store(&source_cache,fixed,"preload",8,RIO_SOURCE_CACHE_LIMIT));
        RioSourceEntry *e=rio_source_find(&source_cache,fixed);e->fixed=1;
        RioSourceCache before=source_cache;
        unsigned count=reads,bytes=read_bytes;interrupt_once=1;
        const unsigned char *data=gangstar_texture_lod_get(0,100+4000000*i,2796344,canonical,&size);
        assert(data && size==699192 && reads>count && read_bytes-bytes==699192);
        assert(!memcmp(data,plan.header,128));
        for(unsigned j=128;j<size;++j)assert(data[j]==((j+2097152)&255));
        assert(!memcmp(&before,&source_cache,sizeof(before)));free((void *)data);
        // A second car read is independent and never displaces fixed preloads.
        count=reads;data=gangstar_texture_lod_get(0,100+4000000*i,2796344,names[i],&size);
        assert(data && reads>count && !memcmp(&before,&source_cache,sizeof(before)));free((void *)data);
#else
        assert(!rio_lod_car_name(names[i]));
        assert(!gangstar_texture_lod_get(0,100+4000000*i,2796344,names[i],&size));
        assert(!reads);
#endif
    }
#ifdef GANGSTAR_CAR_TEXTURE_LOD
    fail_tail=1;uint32_t size=0;assert(!gangstar_texture_lod_get(0,100,2796344,names[0],&size));fail_tail=0;
    unsigned char saved=header[84];header[84]=0;assert(!rio_lod_plan_named(header,2796344,&plan,names[0]));header[84]=saved;
    rio_lod_put(header+12,1024);rio_lod_put(header+16,1024);rio_lod_put(header+28,11);
    assert(!rio_lod_plan_named(header,699192,&plan,names[0])); // Never reduce it again to 512.
    rio_source_trim(&source_cache,0);
#endif
    puts("Car mips: eight exact names, 1024 cap, original ETC bytes, independent readers, unchanged fixed cache, failure fallback and flag-off behavior passed");
}
