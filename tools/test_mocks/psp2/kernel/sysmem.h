#ifndef TEST_VITA_SYSMEM_H
#define TEST_VITA_SYSMEM_H
#include <stddef.h>
typedef int SceUID;
#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RW 1
SceUID sceKernelAllocMemBlock(const char *, int, size_t, void *);
int sceKernelGetMemBlockBase(SceUID, void **);
int sceKernelFreeMemBlock(SceUID);
#endif
