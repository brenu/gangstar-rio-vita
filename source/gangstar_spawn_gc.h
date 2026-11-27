#ifndef GANGSTAR_SPAWN_GC_H
#define GANGSTAR_SPAWN_GC_H
#include <stddef.h>
#include <stdint.h>
/* SpawnMgr::CheckDelayed (ELF 4be78c) queues spawned objects it has no slot
 * for in gToDelete, and the next SpawnMgr::UpdateGarbageCollector (ELF 4be0e4)
 * calls LevelObject::Release (vtable +0x20) on each one. SpawnMgr::Update skips
 * both while spawning is paused, and nothing removes an entry whose object is
 * destroyed by another path first, so a later collection called through freed
 * memory (core dump 1795602707: pc 0x20 after 1 h 40 min, on leaving the game). */
typedef struct { void **begin, **end, **capacity; } GangstarObjectVector;
typedef int (*GangstarVtableCheck)(const void *vtable);
extern GangstarObjectVector *gangstar_spawn_gc_queue;
// Removes every copy of ptr from queue; returns how many were removed.
unsigned gangstar_spawn_gc_erase(GangstarObjectVector *queue, const void *ptr);
// Replacement for UpdateGarbageCollector: releases entries from the back and
// skips nulls and entries whose vtable fails valid. Returns the skipped count.
unsigned gangstar_spawn_gc_collect(GangstarObjectVector *queue, GangstarVtableCheck valid);
void gangstar_spawn_gc_forget_slow(void *ptr);
// Called for each block the game frees. Only the frame thread edits the queue.
static inline void gangstar_spawn_gc_forget(void *ptr) {
    GangstarObjectVector *queue = gangstar_spawn_gc_queue;
    if (ptr && queue && queue->begin != queue->end) gangstar_spawn_gc_forget_slow(ptr);
}
#endif
