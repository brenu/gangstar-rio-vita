#ifndef GANGSTAR_PLATFORM_H
#define GANGSTAR_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint32_t type, bsize;
    uint64_t blocks, bfree, bavail, files, ffree;
    uint32_t fsid[2], namelen, frsize, flags, spare[4];
} GangstarAndroidStatfs;
typedef struct {
    char sysname[65], nodename[65], release[65], version[65], machine[65], domainname[65];
} GangstarAndroidUtsname;
_Static_assert(sizeof(GangstarAndroidStatfs) == 88, "Android ARM statfs size");
_Static_assert(offsetof(GangstarAndroidStatfs, blocks) == 8, "Android statfs blocks");
_Static_assert(offsetof(GangstarAndroidStatfs, bavail) == 24, "Android statfs available");
_Static_assert(offsetof(GangstarAndroidStatfs, frsize) == 60, "Android statfs fragment size");
_Static_assert(sizeof(GangstarAndroidUtsname) == 390, "Android utsname size");
_Static_assert(offsetof(GangstarAndroidUtsname, machine) == 260, "Android utsname machine");
void gangstar_statfs_data(GangstarAndroidStatfs *out, uint64_t total, uint64_t available, uint32_t cluster);
int gangstar_statfs(const char *path, GangstarAndroidStatfs *out);
int gangstar_uname(GangstarAndroidUtsname *out);
int gangstar_nice(int increment);
#endif
