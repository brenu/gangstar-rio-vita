#ifndef GANGSTAR_TEXTURE_HOT_H
#define GANGSTAR_TEXTURE_HOT_H
/* Reload cache for plain (unreduced) archive textures.
 *
 * Districts such as the south favela/mountain free and re-open the same
 * 512x512 "reflect", tile and blend atlases every few hundred frames. They
 * are not reduced-LOD sources, so each reopen read ~171 KiB from the memory
 * card on the frame thread (20-70 ms). An entry is admitted on its second
 * open (or during the loading-screen preload), kept byte-for-byte, and served to the engine as an owned in-memory
 * file, exactly like the existing reduced-texture path.
 *
 * Bounded by RIO_HOT_LIMIT, least-recently-used eviction, and a heap
 * watermark: nothing is stored while the process heap is above it, and the
 * cache shrinks when the game itself needs that memory. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define RIO_HOT_LIMIT (32u * 1024u * 1024u)   // ~191 atlases; generated preload set is capped at 28 MiB
#define RIO_HOT_SLOTS 192u
#define RIO_HOT_SEEN 512u
#define RIO_HOT_MIN_BYTES (64u * 1024u)
#define RIO_HOT_MAX_BYTES (1152u * 1024u) /* patch exterior_favela_alpha.tga is 1048620 */
#define RIO_HOT_HEAP_WATERMARK (176u * 1024u * 1024u)
typedef struct { int archive; uint64_t offset; uint32_t size; } RioHotKey;
typedef struct { RioHotKey key; void *data; uint64_t age; } RioHotEntry;
typedef struct {
    RioHotEntry entries[RIO_HOT_SLOTS];
    RioHotKey seen[RIO_HOT_SEEN]; unsigned seen_next;
    size_t bytes; unsigned count; uint64_t age;
    unsigned hits, admitted, evicted, refused;
} RioHotCache;

static inline int rio_hot_key_equal(RioHotKey a, RioHotKey b) {
    return a.archive == b.archive && a.offset == b.offset && a.size == b.size;
}
static inline int rio_hot_eligible(uint32_t size) {
    return size >= RIO_HOT_MIN_BYTES && size <= RIO_HOT_MAX_BYTES;
}
static inline RioHotEntry *rio_hot_find(RioHotCache *c, RioHotKey key) {
    for (unsigned i = 0; i < RIO_HOT_SLOTS; ++i)
        if (c->entries[i].data && rio_hot_key_equal(c->entries[i].key, key)) {
            c->entries[i].age = ++c->age; return &c->entries[i];
        }
    return NULL;
}
static inline void rio_hot_evict_oldest(RioHotCache *c) {
    unsigned oldest = RIO_HOT_SLOTS;
    for (unsigned i = 0; i < RIO_HOT_SLOTS; ++i)
        if (c->entries[i].data && (oldest == RIO_HOT_SLOTS || c->entries[i].age < c->entries[oldest].age)) oldest = i;
    if (oldest == RIO_HOT_SLOTS) return;
    RioHotEntry *e = &c->entries[oldest];
    c->bytes -= e->key.size; --c->count; ++c->evicted;
    free(e->data); memset(e, 0, sizeof(*e));
}
// Shrink until the cache fits `limit` and the heap (excluding nothing: the
// caller passes current heap use, which includes this cache) is under the
// watermark, or the cache is empty.
static inline void rio_hot_trim(RioHotCache *c, size_t limit, size_t heap_used) {
    while (c->count && (c->bytes > limit || heap_used > RIO_HOT_HEAP_WATERMARK)) {
        size_t before = c->bytes;
        rio_hot_evict_oldest(c);
        size_t freed = before - c->bytes;
        heap_used = heap_used > freed ? heap_used - freed : 0;
    }
}
// First open only records the key; a later open of the same entry admits it.
static inline int rio_hot_note_open(RioHotCache *c, RioHotKey key) {
    for (unsigned i = 0; i < RIO_HOT_SEEN; ++i)
        if (c->seen[i].size && rio_hot_key_equal(c->seen[i], key)) return 1;
    c->seen[c->seen_next] = key; c->seen_next = (c->seen_next + 1) % RIO_HOT_SEEN;
    return 0;
}
// Takes ownership of `data` on success (returns 1). On refusal the caller keeps it.
static inline int rio_hot_store(RioHotCache *c, RioHotKey key, void *data, size_t heap_used) {
    if (!data || !rio_hot_eligible(key.size) || rio_hot_find(c, key)) return 0;
    if (heap_used > RIO_HOT_HEAP_WATERMARK - key.size) {
        rio_hot_trim(c, RIO_HOT_LIMIT, heap_used + key.size);
        ++c->refused; return 0;
    }
    while (c->count && (c->bytes > RIO_HOT_LIMIT - key.size || c->count == RIO_HOT_SLOTS)) rio_hot_evict_oldest(c);
    for (unsigned i = 0; i < RIO_HOT_SLOTS; ++i)
        if (!c->entries[i].data) {
            c->entries[i] = (RioHotEntry){key, data, ++c->age};
            c->bytes += key.size; ++c->count; ++c->admitted; return 1;
        }
    ++c->refused; return 0;
}
#endif
