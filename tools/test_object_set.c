#include "reimpl/bits/object_set.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static GangstarObjectSet objects;
static int storage[7000];
int main(void) {
    for (unsigned i = 0; i < 5000; ++i) assert(gangstar_object_add(&objects, &storage[i]));
    assert(objects.count == 5000);
    for (unsigned i = 0; i < 5000; i += 2) assert(gangstar_object_remove(&objects, &storage[i]));
    for (unsigned i = 0; i < 5000; ++i)
        assert(gangstar_object_contains(&objects, &storage[i]) == (int)(i % 2));
    for (unsigned i = 0; i < 5000; ++i) assert(gangstar_object_add(&objects, &storage[i]));
    assert(objects.count == 5000);
    for (unsigned i = 0; i < 5000; ++i) assert(gangstar_object_remove(&objects, &storage[i]));
    assert(objects.count == 0);
    const void *collisions[32]; unsigned count = 0;
    for (uintptr_t p = 4; count < 32; p += 4) {
        const void *object = (const void *)p;
        if (gangstar_object_hash(object) == GANGSTAR_OBJECT_SET_MASK) collisions[count++] = object;
    }
    for (unsigned i = 0; i < 32; ++i) assert(gangstar_object_add(&objects, collisions[i]));
    for (unsigned i = 0; i < 32; i += 3) assert(gangstar_object_remove(&objects, collisions[i]));
    for (unsigned i = 0; i < 32; ++i)
        assert(gangstar_object_contains(&objects, collisions[i]) == (int)(i % 3 != 0));
    memset(&objects, 0, sizeof(objects));
    unsigned char expected[7000] = {0}; uint32_t state = 1;
    for (unsigned iteration = 0; iteration < 100000; ++iteration) {
        state = state * 1664525u + 1013904223u;
        unsigned i = state % 7000;
        if (expected[i]) { assert(gangstar_object_remove(&objects, &storage[i])); expected[i] = 0; }
        else if (gangstar_object_add(&objects, &storage[i])) expected[i] = 1;
        if (!(iteration % 1000)) for (unsigned j = 0; j < 7000; ++j)
            assert(gangstar_object_contains(&objects, &storage[j]) == expected[j]);
    }
    assert(!gangstar_object_add(&objects, NULL));
    assert(!gangstar_object_contains(&objects, NULL));
    puts("Object registry: >1024 live objects, wrapped collisions, deletion/reuse and lifecycle churn passed");
}
