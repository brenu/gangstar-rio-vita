#ifndef GANGSTAR_STARTUP_TRAMPOLINE_H
#define GANGSTAR_STARTUP_TRAMPOLINE_H
#include <stdint.h>
// Only the simple ARM prologues explicitly verified by the startup profiler.
// Literal LDRs must retain the value read relative to the original PC.
static inline int rio_startup_trampoline(uint32_t out[6], uint32_t address,
    const uint32_t original[2], const uint32_t literals[2]) {
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t word = original[i];
        unsigned rd = (word >> 12) & 15;
        if ((word & 0xffff0000) == 0xe59f0000 && rd != 15) {
            out[i] = (word & 0xfffff000) | 8;
        } else if (((word & 0xffff8000) == 0xe92d0000) ||
            (word == 0xe3073b54 || word == 0xe3403001 || word == 0xed2d8b02 || word == 0xed2d8b04 || word == 0xed2d8b06 || word == 0xe2514000 || word == 0xe2513000 || word == 0xe3a02003 || word == 0xe3500000 || word == 0xe3510000 || word == 0xe3530000 || word == 0xe1530001 || word == 0xe1c022f0 || word == 0xe2801f83) ||
            ((word & 0xffff0ff0) == 0xe1a00000 && rd != 15 && (word & 15) != 15) ||
            ((word & 0xffff0fff) == 0xe3a00000 && rd != 15) ||
            ((word & 0xfffff000) == 0xe24dd000) ||
            (((word & 0xfff00000) == 0xe5900000 || (word & 0xfff00000) == 0xe5d00000) && ((word >> 16) & 15) != 15 && rd != 15)) {
            out[i] = word;
        } else return 0;
        out[4 + i] = literals[i];
    }
    out[2] = 0xe51ff004;
    out[3] = address + 8;
    return 1;
}
#endif
