#ifndef GANGSTAR_ROUTE_CACHE_H
#define GANGSTAR_ROUTE_CACHE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum { RIO_ROUTE_SETS=256, RIO_ROUTE_WAYS=4, RIO_ROUTES=RIO_ROUTE_SETS*RIO_ROUTE_WAYS, RIO_ROUTE_PATH=256 };
typedef struct {
    uintptr_t parent, reader;
    unsigned packed_miss;
    int entry;
    uint64_t stamp, age;
    char name[RIO_ROUTE_PATH], resolved[RIO_ROUTE_PATH];
} RioRoute;
typedef struct { RioRoute entries[RIO_ROUTES]; uint64_t age; } RioRouteCache;
static inline int rio_route_name(const char *name) {
    if (!name || !*name) return 0;
    for (unsigned i = 0; i < RIO_ROUTE_PATH; ++i) if (!name[i]) return 1;
    return 0;
}
// Fingerprint every byte on every check, but mix full words rather than one
// dependent 64-bit multiply per character on the Vita's 32-bit CPU.
static inline int rio_route_mix_path(uint64_t *hash,const char *name) {
    if(!name)return 0;
    size_t length=0;while(length<RIO_ROUTE_PATH && name[length])++length;
    if(length==RIO_ROUTE_PATH)return 0;
    *hash=(*hash ^ length)*UINT64_C(1099511628211);
    size_t at=0;
    while(length-at>=8) {
        uint64_t word;memcpy(&word,name+at,8);
        *hash=(*hash ^ word)*UINT64_C(1099511628211);at+=8;
    }
    uint64_t tail=0;memcpy(&tail,name+at,length-at);
    *hash=(*hash ^ tail)*UINT64_C(1099511628211);
    return 1;
}
static inline unsigned rio_route_set(uintptr_t parent,const char *name) {
    uint32_t h=2166136261u;
    for(const unsigned char *p=(const unsigned char *)name;*p;++p)h=(h^*p)*16777619u;
    h^=(uint32_t)(parent>>4);h^=h>>16;
    return (h%RIO_ROUTE_SETS)*RIO_ROUTE_WAYS;
}
// Main-thread-only packed routing cache. It holds no file objects/positions.
static inline RioRoute *rio_route_find(RioRouteCache *cache, uintptr_t parent,
    uint64_t stamp, const char *name) {
    if (!rio_route_name(name)) return NULL;
    unsigned first=rio_route_set(parent,name);
    for (unsigned i=first;i<first+RIO_ROUTE_WAYS;++i) {
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
    unsigned first=rio_route_set(parent,name),slot=first;
    for (unsigned i=first;i<first+RIO_ROUTE_WAYS;++i) {
        RioRoute *r = &cache->entries[i];
        if ((!r->reader && !r->packed_miss) || (r->parent == parent && !strcmp(r->name, name))) { slot = i; break; }
        if (r->age < cache->entries[slot].age) slot = i;
    }
    RioRoute *r = &cache->entries[slot];
    r->packed_miss = 0; r->entry = -1;
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
