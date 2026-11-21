#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int SceUID;
typedef struct { size_t size; char name[32]; } SceKernelThreadInfo;
static inline SceUID sceKernelGetThreadId(void) { return 1; }
static inline int sceKernelGetThreadInfo(SceUID tid, SceKernelThreadInfo *info) { (void)tid; (void)info; return -1; }
