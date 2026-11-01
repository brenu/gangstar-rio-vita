#ifndef GANGSTAR_DIAGNOSTICS_H
#define GANGSTAR_DIAGNOSTICS_H
#include <stdint.h>
#ifndef GANGSTAR_RELEASE_BUILD
#include <psp2/kernel/processmgr.h>
#endif
static inline uint64_t rio_diagnostic_clock(void) {
#ifdef GANGSTAR_RELEASE_BUILD
    return 0;
#else
    return sceKernelGetProcessTimeWide();
#endif
}
#endif
