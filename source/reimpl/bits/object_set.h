#ifndef GANGSTAR_OBJECT_SET_H
#define GANGSTAR_OBJECT_SET_H
#include <stdint.h>
#include <stddef.h>
#define GANGSTAR_OBJECT_SET_SIZE 8192u
#define GANGSTAR_OBJECT_SET_MASK (GANGSTAR_OBJECT_SET_SIZE - 1u)
typedef struct { const void *slots[GANGSTAR_OBJECT_SET_SIZE]; size_t count; } GangstarObjectSet;
/* External locking covers lookup, native initialization and publication. */
static inline size_t gangstar_object_hash(const void *object) {
    uintptr_t value = (uintptr_t)object >> 2;
    value ^= value >> 13;
    return (value * (uintptr_t)2654435761u) & GANGSTAR_OBJECT_SET_MASK;
}
static inline int gangstar_object_contains(const GangstarObjectSet *set, const void *object) {
    if (!object) return 0;
    size_t slot = gangstar_object_hash(object);
    for (size_t n = 0; n < GANGSTAR_OBJECT_SET_SIZE; ++n) {
        if (set->slots[slot] == object) return 1;
        if (!set->slots[slot]) return 0;
        slot = (slot + 1) & GANGSTAR_OBJECT_SET_MASK;
    }
    return 0;
}
static inline int gangstar_object_add(GangstarObjectSet *set, const void *object) {
    if (!object) return 0;
    if (gangstar_object_contains(set, object)) return 1;
    if (set->count >= GANGSTAR_OBJECT_SET_SIZE * 3 / 4) return 0;
    size_t slot = gangstar_object_hash(object);
    while (set->slots[slot]) slot = (slot + 1) & GANGSTAR_OBJECT_SET_MASK;
    set->slots[slot] = object; ++set->count; return 1;
}
static inline int gangstar_object_remove(GangstarObjectSet *set, const void *object) {
    if (!gangstar_object_contains(set, object)) return 0;
    size_t hole = gangstar_object_hash(object);
    while (set->slots[hole] != object) hole = (hole + 1) & GANGSTAR_OBJECT_SET_MASK;
    set->slots[hole] = NULL;
    size_t next = (hole + 1) & GANGSTAR_OBJECT_SET_MASK;
    while (set->slots[next]) {
        size_t home = gangstar_object_hash(set->slots[next]);
        if (((next - home) & GANGSTAR_OBJECT_SET_MASK) >= ((next - hole) & GANGSTAR_OBJECT_SET_MASK)) {
            set->slots[hole] = set->slots[next]; set->slots[next] = NULL; hole = next;
        }
        next = (next + 1) & GANGSTAR_OBJECT_SET_MASK;
    }
    --set->count; return 1;
}
#endif
