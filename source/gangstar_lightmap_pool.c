#include "gangstar_lightmap_pool.h"
#include "utils/logger.h"
#include <malloc.h>
#include <stdatomic.h>
#include <stdint.h>
static unsigned char *pool;
static atomic_uint used, served, full, peak;
void gangstar_lightmap_pool_init(void) {
    if (pool) return;
    pool = memalign(64, (size_t)RIO_LIGHTMAP_SLOTS * RIO_LIGHTMAP_SLOT_BYTES);
    if (!pool) l_warn("RIO_LIGHTMAP_POOL unavailable=1");
}
void *gangstar_lightmap_pool_get(size_t size) {
    if (!pool || size < RIO_LIGHTMAP_MIN_BYTES || size > RIO_LIGHTMAP_SLOT_BYTES) return NULL;
    unsigned mask = atomic_load_explicit(&used, memory_order_relaxed);
    for (;;) {
        unsigned slot = 0;
        while (slot < RIO_LIGHTMAP_SLOTS && (mask & (1u << slot))) ++slot;
        if (slot == RIO_LIGHTMAP_SLOTS) {
            atomic_fetch_add_explicit(&full, 1, memory_order_relaxed);
            return NULL;
        }
        if (!atomic_compare_exchange_weak_explicit(&used, &mask, mask | (1u << slot),
                memory_order_acquire, memory_order_relaxed)) continue;
        atomic_fetch_add_explicit(&served, 1, memory_order_relaxed);
        unsigned live = (unsigned)__builtin_popcount(mask | (1u << slot));
        unsigned seen = atomic_load_explicit(&peak, memory_order_relaxed);
        while (live > seen && !atomic_compare_exchange_weak(&peak, &seen, live)) {}
        return pool + (size_t)slot * RIO_LIGHTMAP_SLOT_BYTES;
    }
}
int gangstar_lightmap_pool_owns(const void *ptr) {
    return pool && (uintptr_t)ptr >= (uintptr_t)pool &&
        (uintptr_t)ptr - (uintptr_t)pool < (size_t)RIO_LIGHTMAP_SLOTS * RIO_LIGHTMAP_SLOT_BYTES;
}
int gangstar_lightmap_pool_put(void *ptr) {
    if (!gangstar_lightmap_pool_owns(ptr)) return 0;
    size_t slot = ((uintptr_t)ptr - (uintptr_t)pool) / RIO_LIGHTMAP_SLOT_BYTES;
    atomic_fetch_and_explicit(&used, ~(1u << slot), memory_order_release);
    return 1;
}
void gangstar_lightmap_pool_report(void) {
    l_info("RIO_LIGHTMAP_POOL served=%u full=%u peak=%u used_mask=%u", atomic_load(&served),
        atomic_load(&full), atomic_load(&peak), atomic_load(&used));
}
