#include "gangstar_spill_pool.h"
#include "utils/logger.h"
#include <malloc.h>
#include <stdatomic.h>
#include <stdint.h>
static unsigned char *pool;
static atomic_uint used, served;
void gangstar_spill_init(void) {
    if (pool) return;
    pool = memalign(4096, (size_t)RIO_SPILL_SLOTS * RIO_SPILL_SLOT_BYTES);
    if (!pool) l_warn("RIO_SPILL_POOL unavailable=1");
}
void *gangstar_spill_get(size_t alignment, size_t size) {
    if (!pool || alignment > 4096 || size < RIO_SPILL_MIN_BYTES || size > RIO_SPILL_SLOT_BYTES) return NULL;
    unsigned mask = atomic_load_explicit(&used, memory_order_relaxed);
    for (;;) {
        unsigned slot = 0;
        while (slot < RIO_SPILL_SLOTS && (mask & (1u << slot))) ++slot;
        if (slot == RIO_SPILL_SLOTS) return NULL;
        if (!atomic_compare_exchange_weak_explicit(&used, &mask, mask | (1u << slot),
                memory_order_acquire, memory_order_relaxed)) continue;
        unsigned count = atomic_fetch_add_explicit(&served, 1, memory_order_relaxed) + 1;
        if (count <= 32)
            l_warn("RIO_SPILL_POOL served=%u size=%u used_mask=%u", count, (unsigned)size, mask | (1u << slot));
        return pool + (size_t)slot * RIO_SPILL_SLOT_BYTES;
    }
}
size_t gangstar_spill_usable(const void *ptr) {
    if (!pool || (uintptr_t)ptr < (uintptr_t)pool) return 0;
    size_t offset = (uintptr_t)ptr - (uintptr_t)pool;
    if (offset >= (size_t)RIO_SPILL_SLOTS * RIO_SPILL_SLOT_BYTES) return 0;
    return RIO_SPILL_SLOT_BYTES - offset % RIO_SPILL_SLOT_BYTES;
}
int gangstar_spill_put(void *ptr) {
    if (!gangstar_spill_usable(ptr)) return 0;
    size_t slot = ((uintptr_t)ptr - (uintptr_t)pool) / RIO_SPILL_SLOT_BYTES;
    atomic_fetch_and_explicit(&used, ~(1u << slot), memory_order_release);
    return 1;
}
