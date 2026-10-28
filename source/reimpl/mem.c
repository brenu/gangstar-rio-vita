/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/mem.h"
#include "utils/logger.h"

#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdatomic.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <kubridge.h>
#include "gangstar_memory_policy.h"
#include <psp2/kernel/clib.h>

void *sceClibMemclr(void *dst, size_t len) {
    return sceClibMemset(dst, 0, len);
}

void *valloc_soloader(size_t length) {
    void *result = valloc(length);
    int saved_errno = errno;
    if (!result || length >= 1024 * 1024) {
        SceKernelFreeMemorySizeInfo available = {.size = sizeof(available)};
        sceKernelGetFreeMemorySize(&available);
        l_info("Guest valloc: size=%u result=%p errno=%d; libc free=%u user free=%u",
                (unsigned)length, result, result ? 0 : saved_errno,
                (unsigned)mallinfo().fordblks, available.size_user);
    }
    errno = saved_errno;
    return result;
}

/* Android anonymous mappings require page permissions. A malloc
 * replacement cannot provide page isolation, permissions, or executable code.
 */
#define GANGSTAR_MAX_MAPPINGS 64
typedef struct { void *base; size_t size; SceUID uid; } GangstarMapping;
static GangstarMapping mappings[GANGSTAR_MAX_MAPPINGS];
static atomic_flag mappings_lock = ATOMIC_FLAG_INIT;

static void lock_mappings(void) {
    while (atomic_flag_test_and_set_explicit(&mappings_lock, memory_order_acquire))
        sceKernelDelayThread(100);
}
static void unlock_mappings(void) {
    atomic_flag_clear_explicit(&mappings_lock, memory_order_release);
}
static unsigned int vita_protection(int prot) {
    unsigned int result = 0;
    if (prot & 1) result |= KU_KERNEL_PROT_READ;
    if (prot & 2) result |= KU_KERNEL_PROT_WRITE;
    if (prot & 4) result |= KU_KERNEL_PROT_EXEC;
    return result;
}

static int protect_pages(void *addr, size_t length, int prot) {
    // Establish 4 KiB kernel page records before guard/RX changes. Protecting
    // an initial large RW block and then splitting it can remap neighboring
    // pieces with the block's original RW permissions in kubridge. One page
    // per operation also prevents its multi-page merge path from undoing this.
    for (size_t offset = 0; offset < length; offset += 4096) {
        int result = kuKernelMemProtect((char *)addr + offset, 4096, vita_protection(prot));
        if (result < 0) {
            l_error("Page protection failed: address=%p prot=%d result=0x%x",
                    (char *)addr + offset, prot, (unsigned)result);
            return result;
        }
    }
    return 0;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offs) {
    (void)addr; // A non-fixed address is only a hint under mmap semantics.
    size_t rounded;
    if (!gangstar_page_size(length, &rounded) || (prot & ~7)) { errno = EINVAL; return MAP_FAILED; }
    if (!gangstar_anonymous_flags(flags) || fd != -1 || offs != 0) {
        l_error("Unsupported mmap flags=0x%x fd=%d offset=%ld", flags, fd, (long)offs);
        errno = ENOTSUP; return MAP_FAILED;
    }
    lock_mappings();
    int slot;
    for (slot = 0; slot < GANGSTAR_MAX_MAPPINGS && mappings[slot].base; ++slot) {}
    if (slot == GANGSTAR_MAX_MAPPINGS) { unlock_mappings(); errno = ENOMEM; return MAP_FAILED; }
    SceUID uid = sceKernelAllocMemBlock("gangstar_mmap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, rounded, NULL);
    void *base = NULL;
    if (uid < 0 || sceKernelGetMemBlockBase(uid, &base) < 0) {
        if (uid >= 0) sceKernelFreeMemBlock(uid);
        unlock_mappings(); errno = ENOMEM; return MAP_FAILED;
    }
    memset(base, 0, rounded);
    if (protect_pages(base, rounded, prot) < 0) {
        sceKernelFreeMemBlock(uid);
        unlock_mappings(); errno = EACCES; return MAP_FAILED;
    }
    mappings[slot] = (GangstarMapping){base, rounded, uid};
    unlock_mappings();
    l_debug("mmap: %p size=%u prot=%d", base, (unsigned)rounded, prot);
    return base;
}

int mprotect_soloader(void *addr, size_t length, int prot) {
    size_t rounded;
    if (((uintptr_t)addr & 4095) || (prot & ~7)) { errno = EINVAL; return -1; }
    if (!length) return 0;
    if (!gangstar_page_size(length, &rounded)) { errno = EINVAL; return -1; }
    lock_mappings();
    int slot;
    for (slot = 0; slot < GANGSTAR_MAX_MAPPINGS; ++slot)
        if (mappings[slot].base && gangstar_range_contains((uintptr_t)mappings[slot].base,
                mappings[slot].size, (uintptr_t)addr, rounded)) break;
    if (slot == GANGSTAR_MAX_MAPPINGS) { unlock_mappings(); errno = ENOMEM; return -1; }
    int result = protect_pages(addr, rounded, prot);
    if (result >= 0 && (prot & 4)) kuKernelFlushCaches(addr, rounded);
    unlock_mappings();
    if (result < 0) { errno = EACCES; return -1; }
    l_debug("mprotect: %p size=%u prot=%d; 4 KiB pages", addr, (unsigned)rounded, prot);
    return 0;
}

int munmap(void *addr, size_t length) {
    size_t rounded;
    if (((uintptr_t)addr & 4095) || !gangstar_page_size(length, &rounded)) { errno = EINVAL; return -1; }
    lock_mappings();
    for (int slot = 0; slot < GANGSTAR_MAX_MAPPINGS; ++slot) {
        if (mappings[slot].base == addr && mappings[slot].size == rounded) {
            int result = sceKernelFreeMemBlock(mappings[slot].uid);
            if (result >= 0) memset(&mappings[slot], 0, sizeof(mappings[slot]));
            unlock_mappings();
            if (result < 0) { errno = EINVAL; return -1; }
            return 0;
        }
    }
    unlock_mappings();
    // Partial unmapping/file mappings are not implemented. Never free a heap
    // pointer or claim that an unsupported unmap succeeded.
    errno = EINVAL; return -1;
}
