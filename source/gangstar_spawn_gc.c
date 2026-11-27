#include "gangstar_spawn_gc.h"
#include "utils/logger.h"
#include <stdatomic.h>
int gangstar_wait_is_main(void);
GangstarObjectVector *gangstar_spawn_gc_queue;
static atomic_uint forgotten, foreign, skipped;

unsigned gangstar_spawn_gc_erase(GangstarObjectVector *queue, const void *ptr) {
    void **out = queue->begin;
    for (void **in = queue->begin; in != queue->end; ++in)
        if (*in != ptr) *out++ = *in;
    unsigned removed = (unsigned)(queue->end - out);
    queue->end = out;
    return removed;
}

unsigned gangstar_spawn_gc_collect(GangstarObjectVector *queue, GangstarVtableCheck valid) {
    unsigned rejected = 0;
    // Pop before Release, as the original does: Release frees the object, and
    // gangstar_spawn_gc_forget then drops any remaining copy of it.
    while (queue->begin != queue->end) {
        void *object = *--queue->end;
        if (!object) continue;
        void **vtable = *(void ***)object;
        if (!valid(vtable)) { ++rejected; continue; }
        ((void (*)(void *))vtable[8])(object);
    }
    if (rejected) {
        unsigned total = atomic_fetch_add_explicit(&skipped, rejected, memory_order_relaxed) + rejected;
        if (total <= 32) l_warn("RIO_SPAWN_GC skipped_invalid=%u total=%u", rejected, total);
    }
    return rejected;
}

void gangstar_spawn_gc_forget_slow(void *ptr) {
    GangstarObjectVector *queue = gangstar_spawn_gc_queue;
    if (!gangstar_wait_is_main()) {
        // Another thread must not edit the vector; collect's vtable check is
        // the fallback. Only report actual queued objects.
        for (void **entry = queue->begin; entry != queue->end; ++entry)
            if (*entry == ptr) {
                if (atomic_fetch_add_explicit(&foreign, 1, memory_order_relaxed) < 8)
                    l_warn("RIO_SPAWN_GC queued object %p freed off the frame thread", ptr);
                break;
            }
        return;
    }
    unsigned removed = gangstar_spawn_gc_erase(queue, ptr);
    if (removed) {
        unsigned total = atomic_fetch_add_explicit(&forgotten, removed, memory_order_relaxed) + removed;
        if (total <= 32) l_warn("RIO_SPAWN_GC dropped freed object %p copies=%u total=%u", ptr, removed, total);
    }
}
