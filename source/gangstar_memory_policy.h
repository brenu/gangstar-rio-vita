#ifndef GANGSTAR_MEMORY_POLICY_H
#define GANGSTAR_MEMORY_POLICY_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

static inline int gangstar_page_size(size_t length, size_t *rounded) {
    if (!length || length > SIZE_MAX - 4095) return 0;
    *rounded = (length + 4095) & ~(size_t)4095;
    return 1;
}

static inline int gangstar_range_contains(uintptr_t base, size_t size, uintptr_t address, size_t length) {
    return address >= base && length <= size && address - base <= size - length;
}

/* The observed runtime uses MAP_SHARED|MAP_ANONYMOUS (0x21). */
static inline int gangstar_anonymous_flags(int flags) {
    int sharing = flags & 3;
    return (flags & ~0x23) == 0 && (flags & 0x20) && (sharing == 1 || sharing == 2);
}
#endif
