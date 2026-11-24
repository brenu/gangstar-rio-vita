#include "gangstar_memory_relief.h"
#include "gangstar_wait_perf.h"
#include "utils/logger.h"
#include <errno.h>
#include <stdatomic.h>
#define RIO_RELIEF_STEP (8u * 1024u * 1024u)
static atomic_uint relieved;
size_t gangstar_memory_relieve(size_t needed) {
    int saved_errno = errno;
    size_t target = needed > RIO_RELIEF_STEP ? needed : RIO_RELIEF_STEP;
    size_t freed = 0;
#ifdef GANGSTAR_TEXTURE_HOT_CACHE
    if (gangstar_wait_is_main()) freed = gangstar_texture_hot_relieve(target);
#endif
#ifdef GANGSTAR_SMALL_LIGHTMAPS
    if (freed < target) freed += gangstar_texture_source_relieve(target - freed);
#endif
    if (freed) {
        unsigned count = atomic_fetch_add_explicit(&relieved, 1, memory_order_relaxed) + 1;
        if (count <= 32)
            l_warn("RIO_MEMORY_RELIEF count=%u needed=%u freed=%u", count, (unsigned)needed, (unsigned)freed);
    }
    errno = saved_errno;
    return freed;
}
unsigned gangstar_memory_relief_count(void) {
    return atomic_load_explicit(&relieved, memory_order_relaxed);
}
