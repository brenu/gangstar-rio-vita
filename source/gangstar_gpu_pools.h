#ifndef GANGSTAR_GPU_POOLS_H
#define GANGSTAR_GPU_POOLS_H
#include <stddef.h>
typedef struct { unsigned ram_mib, physical_mib; } RioGpuPools;
/* vitaGL's three rotating vertex buffers take 10.7 MiB each. One that fits
 * neither CPU pool spills into CDRAM, taking that space from textures. Late in
 * a session that pushes 2048x2048 lightmaps out of VRAM into the game heap,
 * and a failed lightmap upload shows as green/pink scenery.
 * A 23 MiB phycont pool holds two buffers. Plugins can leave less phycont than
 * the usual 26 MiB, so use what there is, keeping 3 MiB free; 11 MiB is the
 * least that holds one buffer. */
static inline RioGpuPools rio_gpu_pools(size_t user, size_t phycont) {
    const size_t mib = 1024u * 1024u;
    RioGpuPools pools = {16, 8};
    if (user < 32 * mib || phycont < 14 * mib) return pools;
    pools.ram_mib = 24;
    pools.physical_mib = (unsigned)(phycont / mib) - 3;
    if (pools.physical_mib > 23) pools.physical_mib = 23;
    return pools;
}
#endif
