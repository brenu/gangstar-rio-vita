#ifndef GANGSTAR_HEAP_USAGE_H
#define GANGSTAR_HEAP_USAGE_H
/* Heap-used readings for the port's memory guards.
 *
 * newlib's mallinfo() holds the global malloc lock while it walks every free
 * chunk, so every other thread's malloc/free (the audio thread's VoxAlloc
 * included) waits for the whole walk. Guards that only need "roughly how full
 * is the heap" use the recent reading, which calls mallinfo() at most once per
 * RIO_HEAP_RECENT_US or RIO_HEAP_RECENT_CALLS readings. Use the exact reading
 * only where a burst of allocations must see its own effect immediately. */
#include <stddef.h>
#define RIO_HEAP_RECENT_US 50000u
#define RIO_HEAP_RECENT_CALLS 8u
#ifdef __vita__
size_t gangstar_heap_used_exact(void);
size_t gangstar_heap_used_recent(void);
#endif
#endif
