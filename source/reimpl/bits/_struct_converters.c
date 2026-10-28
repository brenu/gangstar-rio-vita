/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  _struct_converters.c
 * @brief Converters for `dirent` struct, `stat` struct, and `open()` flags
 *        that deal with newlib (Vita) and bionic (Android) incompatibilities.
 *
 * This file has to be `#include`d in `reimpl/io.c` and not compiled on its own.
 */

#define SC_INLINE static inline __attribute__((always_inline))

#define BIONIC_O_WRONLY                                          01
#define BIONIC_O_RDWR                                            02
#define BIONIC_O_CREAT                                         0100
#define BIONIC_O_EXCL                                          0200
#define BIONIC_O_TRUNC                                        01000
#define BIONIC_O_APPEND                                       02000
#define BIONIC_O_NONBLOCK                                     04000
#define BIONIC_O_DIRECTORY                                 00200000
#define BIONIC__O_TMPFILE                                 020000000
#define BIONIC_O_TMPFILE   (BIONIC__O_TMPFILE | BIONIC_O_DIRECTORY)

/**
 * Convert bionic (Android) `open()` flags to newlib (Vita) flags
 *
 * @param[in] flags open() flags created using musl defines
 *
 * @return open(flags) recreated using newlib defines
 */
SC_INLINE int oflags_bionic_to_newlib(int flags) {
    int out = 0;
    if (flags & BIONIC_O_RDWR)
        out |= O_RDWR;
    else if (flags & BIONIC_O_WRONLY)
        out |= O_WRONLY;
    else
        out |= O_RDONLY;
    if (flags & BIONIC_O_NONBLOCK)
        out |= O_NONBLOCK;
    if (flags & BIONIC_O_APPEND)
        out |= O_APPEND;
    if ((flags & BIONIC_O_CREAT) || (flags & BIONIC_O_TMPFILE))
        out |= O_CREAT;
    if (flags & BIONIC_O_TRUNC)
        out |= O_TRUNC;
    if (flags & BIONIC_O_EXCL)
        out |= O_EXCL;
    return out;
}

/**
 * Convert newlib (Vita) `dirent` struct to bionic (Android) format.
 *
 * @param[in] dirent_newlib Pointer to a newlib-format dirent struct
 *
 * @return Pointer to a bionic-format dirent struct.
 *         Must be freed by the caller.
 */
SC_INLINE
dirent64_bionic * dirent_newlib_to_bionic(const struct dirent* dirent_newlib) {
    dirent64_bionic * ret = malloc(sizeof(dirent64_bionic));
    strncpy(ret->d_name, dirent_newlib->d_name, sizeof(ret->d_name));
    ret->d_off = 0;
    ret->d_reclen = 0;
    ret->d_type = SCE_S_ISDIR(dirent_newlib->d_stat.st_mode) ? DT_DIR : DT_REG;
    return ret;
}

/**
 * Convert newlib (Vita) `stat` struct to bionic (Android) format.
 * @param[in]  src Pointer to a newlib-format stat struct
 * @param[out] dst Pointer to a bionic-format stat struct
 */
#include "reimpl/bits/_stat_converter.h"
