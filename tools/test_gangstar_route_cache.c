#include "gangstar_route_cache.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    // Every byte, boundary and length must affect the configuration stamp.
    char path[RIO_ROUTE_PATH+1];
    for(unsigned len=0;len<RIO_ROUTE_PATH;++len) {
        memset(path,'a',len);path[len]=0;
        uint64_t base=123,again=123;assert(rio_route_mix_path(&base,path));
        assert(rio_route_mix_path(&again,path) && base==again);
        for(unsigned at=0;at<len;++at) {
            path[at]='b';uint64_t changed=123;
            assert(rio_route_mix_path(&changed,path) && changed!=base);path[at]='a';
        }
        if(len) {path[len-1]=0;uint64_t shorter=123;assert(rio_route_mix_path(&shorter,path) && shorter!=base);}
    }
    memset(path,'a',sizeof(path));uint64_t hash=123;
    assert(!rio_route_mix_path(&hash,path) && !rio_route_mix_path(&hash,NULL));
    static RioRouteCache cache;
    assert(!rio_route_find(&cache, 1, 10, "sound.wav"));
    rio_route_store(&cache, 1, 10, "sound.wav", 100, "audio/sound.wav");
    RioRoute *r = rio_route_find(&cache, 1, 10, "sound.wav");
    assert(r && r->reader == 100 && r->entry==-1 && !strcmp(r->resolved, "audio/sound.wav"));
    r->entry=17;
    assert(rio_route_find(&cache,1,10,"sound.wav")->entry==17);
    rio_route_store(&cache,1,12,"sound.wav",200,"patch/sound.wav");
    assert(!rio_route_find(&cache,1,10,"sound.wav"));
    assert(rio_route_find(&cache,1,12,"sound.wav")->entry==-1);
    rio_route_store(&cache,1,10,"sound.wav",100,"audio/sound.wav");
    assert(!rio_route_find(&cache, 2, 10, "sound.wav")); // Different filesystem.
    assert(!rio_route_find(&cache, 1, 11, "sound.wav")); // Changed archive/folder order.
    assert(!rio_route_find(&cache, 1, 10, "Sound.wav")); // Exact key, no new normalization.
    rio_route_store(&cache, 1, 11, "sound.wav", 200, "override/sound.wav");
    r = rio_route_find(&cache, 1, 11, "sound.wav"); assert(r && r->reader == 200);
    r->reader = 0; assert(!rio_route_find(&cache, 1, 11, "sound.wav"));
    char too_long[RIO_ROUTE_PATH + 1]; memset(too_long, 'a', sizeof(too_long) - 1); too_long[RIO_ROUTE_PATH] = 0;
    rio_route_store(&cache, 1, 11, too_long, 100, "x"); assert(!rio_route_find(&cache, 1, 11, too_long));
    rio_route_store(&cache, 1, 11, "missing", 0, "x"); assert(!rio_route_find(&cache, 1, 11, "missing"));
    rio_route_store_packed_miss(&cache,1,11,"optional.tga");
    r = rio_route_find(&cache,1,11,"optional.tga"); assert(r && r->packed_miss && !r->reader);
    assert(!rio_route_find(&cache,1,12,"optional.tga"));
    assert(!rio_route_find(&cache,2,11,"optional.tga"));
    rio_route_store(&cache,1,11,"optional.tga",100,"_packetc/optional.tga");
    r = rio_route_find(&cache,1,11,"optional.tga"); assert(r && !r->packed_miss && r->reader==100);
    rio_route_store_packed_miss(&cache,1,11,"optional.tga");
    r = rio_route_find(&cache,1,11,"optional.tga"); assert(r && r->packed_miss && !r->reader);
    // Force a set collision and verify per-set LRU without a global scan.
    char collisions[RIO_ROUTE_WAYS+1][40];unsigned found=0;
    unsigned set=rio_route_set(3,"asset-0");
    for(unsigned i=0;found<RIO_ROUTE_WAYS+1;++i) {
        char name[40];snprintf(name,sizeof(name),"asset-%u",i);
        if(rio_route_set(3,name)==set)strcpy(collisions[found++],name);
    }
    for(unsigned i=0;i<RIO_ROUTE_WAYS;++i)
        rio_route_store(&cache,3,20,collisions[i],300,collisions[i]);
    assert(rio_route_find(&cache,3,20,collisions[0]));
    rio_route_store(&cache,3,20,collisions[RIO_ROUTE_WAYS],300,collisions[RIO_ROUTE_WAYS]);
    assert(!rio_route_find(&cache,3,20,collisions[1]));
    assert(rio_route_find(&cache,3,20,collisions[0]));
    assert(rio_route_find(&cache,3,20,collisions[RIO_ROUTE_WAYS]));
    puts("Archive routes: exact keys, filesystem isolation, configuration invalidation, packed-only misses, path bounds and LRU passed");
}
