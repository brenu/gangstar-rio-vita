#include "gangstar_route_cache.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    static RioRouteCache cache;
    assert(!rio_route_find(&cache, 1, 10, "sound.wav"));
    rio_route_store(&cache, 1, 10, "sound.wav", 100, "audio/sound.wav");
    RioRoute *r = rio_route_find(&cache, 1, 10, "sound.wav");
    assert(r && r->reader == 100 && !strcmp(r->resolved, "audio/sound.wav"));
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
    for (unsigned i = 0; i < RIO_ROUTES + 1; ++i) {
        char name[40]; snprintf(name, sizeof(name), "asset-%u", i);
        rio_route_store(&cache, 3, 20, name, 300, name);
    }
    assert(!rio_route_find(&cache, 3, 20, "asset-0"));
    assert(rio_route_find(&cache, 3, 20, "asset-128"));
    puts("Archive routes: exact keys, filesystem isolation, configuration invalidation, packed-only misses, path bounds and LRU passed");
}
