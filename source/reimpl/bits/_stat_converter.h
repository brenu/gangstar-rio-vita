#ifndef GANGSTAR_STAT_CONVERTER_H
#define GANGSTAR_STAT_CONVERTER_H
#include <sys/stat.h>
#include <string.h>
#include "gangstar_android_stat.h"
static inline void stat_newlib_to_bionic(const struct stat *src, stat64_bionic *dst) {
    memset(dst, 0, sizeof(*dst));
    dst->st_dev = src->st_dev;
    dst->__st_ino = src->st_ino;
    dst->st_ino = src->st_ino;
    dst->st_mode = src->st_mode;
    dst->st_nlink = src->st_nlink;
    dst->st_uid = src->st_uid;
    dst->st_gid = src->st_gid;
    dst->st_rdev = src->st_rdev;
    dst->st_size = src->st_size;
    dst->st_blksize = src->st_blksize;
    dst->st_blocks = src->st_blocks;
    dst->st_atim.tv_sec = src->st_atime;
    dst->st_mtim.tv_sec = src->st_mtime;
    dst->st_ctim.tv_sec = src->st_ctime;
}
#endif
