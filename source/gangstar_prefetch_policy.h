#ifndef GANGSTAR_PREFETCH_POLICY_H
#define GANGSTAR_PREFETCH_POLICY_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint64_t end, target; size_t consecutive; } RioPrefetchPolicy;
static inline void rio_prefetch_reset(RioPrefetchPolicy *p) {
    *p = (RioPrefetchPolicy){.target = UINT64_MAX};
}
// Predict only the next block of a demonstrated sequential stream.
static inline int rio_prefetch_next(RioPrefetchPolicy *p, uint64_t offset,
    size_t count, uint64_t length, size_t block, uint64_t *target) {
    if (!count || offset >= length || count > length - offset || !block) {
        rio_prefetch_reset(p); return 0;
    }
    size_t run = offset == p->end ? p->consecutive : 0;
    if (offset != p->end) p->target = UINT64_MAX;
    p->consecutive = count >= 65536 || run >= 65536 - count ? 65536 : run + count;
    p->end = offset + count;
    uint64_t next = p->end / block * block;
    if (p->end % block) next += block;
    if (p->consecutive < 65536 || next >= length || next == p->target) return 0;
    p->target = *target = next;
    return 1;
}
// Only bounded, nonempty immutable archive ranges; subtraction avoids overflow.
static inline int rio_prefetch_asset_bounds(uint64_t offset, uint64_t size,
    uint64_t length, size_t block, uint64_t *first, uint64_t *end) {
    if (!block || size < 256 * 1024 || size > 4 * 1024 * 1024 ||
        offset >= length || size > length - offset) return 0;
    *first = offset / block * block;
    *end = offset + size;
    return 1;
}
static inline int rio_prefetch_advance(uint64_t *offset, uint64_t end, size_t block) {
    if (!block || *offset >= end || end - *offset <= block) return 0;
    *offset += block;
    return 1;
}
#endif
