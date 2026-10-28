#include "gangstar_platform.h"
#include <string.h>

void gangstar_statfs_data(GangstarAndroidStatfs *out, uint64_t total, uint64_t available, uint32_t cluster) {
    memset(out, 0, sizeof(*out));
    if (!cluster) cluster = 4096;
    out->bsize = out->frsize = cluster;
    out->blocks = total / cluster;
    out->bfree = out->bavail = available / cluster;
}
