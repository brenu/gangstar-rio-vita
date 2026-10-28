#include "gangstar_memory_policy.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    size_t rounded = 0;
    assert(!gangstar_page_size(0, &rounded));
    assert(gangstar_page_size(1, &rounded) && rounded == 4096);
    assert(gangstar_page_size(4096, &rounded) && rounded == 4096);
    assert(gangstar_page_size(4097, &rounded) && rounded == 8192);
    assert(!gangstar_page_size(SIZE_MAX, &rounded));
    assert(gangstar_range_contains(0x10000, 0x4000, 0x10000, 0x1000));
    assert(gangstar_range_contains(0x10000, 0x4000, 0x13000, 0x1000));
    assert(!gangstar_range_contains(0x10000, 0x4000, 0xf000, 0x1000));
    assert(!gangstar_range_contains(0x10000, 0x4000, 0x13000, 0x2000));
    assert(!gangstar_range_contains(0x10000, 0x4000, UINTPTR_MAX, 4096));
    assert(gangstar_anonymous_flags(0x21));
    assert(gangstar_anonymous_flags(0x22));
    assert(!gangstar_anonymous_flags(0x20));
    assert(!gangstar_anonymous_flags(0x23));
    assert(!gangstar_anonymous_flags(0x31)); // MAP_FIXED rejected
    assert(!gangstar_anonymous_flags(0x1));  // file-backed mapping rejected
    puts("Memory policy: page rounding, overflow, guard ranges and supported mmap flags passed");
}
