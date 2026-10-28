#ifndef GANGSTAR_ANDROID_STAT_H
#define GANGSTAR_ANDROID_STAT_H
#include <stdint.h>
#include <stddef.h>
typedef struct { int32_t tv_sec; int32_t tv_nsec; } GangstarAndroidTimespec;
// Android ARM EABI stat layout. Keep 64-bit sizes and fixed-width fields;
// host longs and the native Vita stat structure have different layouts.
typedef struct __attribute__((packed)) {
    uint64_t st_dev;
    uint8_t __pad0[4];
    uint32_t __st_ino, st_mode, st_nlink, st_uid, st_gid;
    uint64_t st_rdev;
    uint8_t __pad3[8];
    int64_t st_size;
    uint32_t st_blksize, __pad4;
    uint64_t st_blocks;
    GangstarAndroidTimespec st_atim, st_mtim, st_ctim;
    uint64_t st_ino;
} stat64_bionic;
_Static_assert(sizeof(stat64_bionic) == 104, "Android ARM stat size");
_Static_assert(offsetof(stat64_bionic, st_mode) == 16, "Android mode offset");
_Static_assert(offsetof(stat64_bionic, st_size) == 48, "Android size offset");
_Static_assert(offsetof(stat64_bionic, st_blocks) == 64, "Android block count offset");
_Static_assert(offsetof(stat64_bionic, st_mtim) == 80, "Android mtime offset");
#endif
