#ifndef GANGSTAR_NATIVE_PERF_H
#define GANGSTAR_NATIVE_PERF_H
#include <stdint.h>
typedef struct {
    uint64_t lookup, dds, animation, resources, world, graphics, streaming, file_load;
    const char *top; uint64_t top_us; unsigned top_calls;
} GangstarNativePerf;
// Nested timings: these fields must not be added together.
GangstarNativePerf gangstar_native_stats(void);
#endif
