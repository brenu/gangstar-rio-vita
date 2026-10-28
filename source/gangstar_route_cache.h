#ifndef GANGSTAR_ROUTE_CACHE_H
#define GANGSTAR_ROUTE_CACHE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum { RIO_ROUTES = 128, RIO_ROUTE_PATH = 256 };
typedef struct {
    uintptr_t parent, reader;
    unsigned packed_miss;
    uint64_t stamp, age;
    char name[RIO_ROUTE_PATH], resolved[RIO_ROUTE_PATH];
} RioRoute;
typedef struct { RioRoute entries[RIO_ROUTES]; uint64_t age; } RioRouteCache;
static inline int rio_route_name(const char *name) {
    if (!name || !*name) return 0;
    for (unsigned i = 0; i < RIO_ROUTE_PATH; ++i) if (!name[i]) return 1;
    return 0;
}
// Main-thread-only packed routing cache. It holds no file objects/positions.
static inline RioRoute *rio_route_find(RioRouteCache *cache, uintptr_t parent,
    uint64_t stamp, const char *name) {
    if (!rio_route_name(name)) return NULL;
    for (unsigned i = 0; i < RIO_ROUTES; ++i) {
        RioRoute *r = &cache->entries[i];
        if ((r->reader || r->packed_miss) && r->parent == parent && r->stamp == stamp && !strcmp(r->name, name)) {
            r->age = ++cache->age; return r;
        }
    }
    return NULL;
}
static inline void rio_route_store(RioRouteCache *cache, uintptr_t parent,
    uint64_t stamp, const char *name, uintptr_t reader, const char *resolved) {
    if (!reader || !rio_route_name(name) || !rio_route_name(resolved)) return;
    unsigned slot = 0;
    for (unsigned i = 0; i < RIO_ROUTES; ++i) {
        RioRoute *r = &cache->entries[i];
        if ((!r->reader && !r->packed_miss) || (r->parent == parent && !strcmp(r->name, name))) { slot = i; break; }
        if (r->age < cache->entries[slot].age) slot = i;
    }
    RioRoute *r = &cache->entries[slot];
    r->packed_miss = 0;
    r->parent = parent; r->stamp = stamp; r->reader = reader; r->age = ++cache->age;
    strcpy(r->name, name); strcpy(r->resolved, resolved);
}
// Only cache absence in immutable packed readers; loose files must be retried.
static inline void rio_route_store_packed_miss(RioRouteCache *cache, uintptr_t parent,
    uint64_t stamp, const char *name) {
    rio_route_store(cache, parent, stamp, name, UINTPTR_MAX, name);
    RioRoute *r = rio_route_find(cache, parent, stamp, name);
    if (r) { r->reader = 0; r->packed_miss = 1; }
}
#endif
