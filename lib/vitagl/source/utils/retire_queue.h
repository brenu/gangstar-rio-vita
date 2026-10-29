#ifndef VGL_RETIRE_QUEUE_H
#define VGL_RETIRE_QUEUE_H
#include <stddef.h>

/* Allocation failure is not an end marker. A queue can contain holes after
 * an unsuccessful transient allocation; every later live entry still owns
 * memory until its normal GPU retirement frame. */
static inline void vgl_retire_queue_collect(void **entries, size_t capacity,
    void (*release)(void *)) {
    for (size_t i = 0; i < capacity; ++i) {
        void *entry = entries[i];
        entries[i] = NULL;
        if (entry) release(entry);
    }
}
#endif
