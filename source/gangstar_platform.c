#include "gangstar_platform.h"
#include "utils/logger.h"
#include <errno.h>
#include <string.h>
#include <psp2/io/devctl.h>
#include <psp2/io/stat.h>

int gangstar_statfs(const char *path, GangstarAndroidStatfs *out) {
    if (!path || !out) { errno = EFAULT; return -1; }
    if (!strncmp(path, "raw:/", 5)) path += 5;
    // Until the general file-path bridge is implemented, only native mount
    // paths are accepted; Android paths must not report another disk's size.
    const char *colon = strchr(path, ':');
    if (!colon || colon == path || colon - path >= 15) { errno = ENOENT; return -1; }
    for (const char *p = path; p < colon; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) {
            errno = ENOENT; return -1;
        }
    SceIoStat st;
    if (sceIoGetstat(path, &st) < 0) { errno = ENOENT; return -1; }
    char mount[16];
    size_t size = (size_t)(colon - path) + 1;
    memcpy(mount, path, size); mount[size] = 0;
    SceIoDevInfo info = {0};
    if (sceIoDevctl(mount, 0x3001, NULL, 0, &info, sizeof(info)) < 0 ||
            info.max_size < 0 || info.free_size < 0) { errno = EIO; return -1; }
    gangstar_statfs_data(out, (uint64_t)info.max_size, (uint64_t)info.free_size, info.cluster_size);
    return 0;
}

int gangstar_uname(GangstarAndroidUtsname *out) {
    if (!out) { errno = EFAULT; return -1; }
    memset(out, 0, sizeof(*out));
    strcpy(out->sysname, "PSVita");
    strcpy(out->nodename, "psvita");
    strcpy(out->release, "unknown");
    strcpy(out->version, "unknown");
    strcpy(out->machine, "armv7l");
    return 0;
}

int gangstar_nice(int increment) {
    // The observed caller ignores the result of nice(5). Do not map Linux
    // niceness values directly to the differently scaled Vita priorities.
    l_warn("nice(%d): Linux scheduling adjustment is unsupported", increment);
    errno = ENOSYS;
    return -1;
}
