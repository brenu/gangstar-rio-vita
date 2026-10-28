#define _POSIX_C_SOURCE 200809L
#include "reimpl/bits/_stat_converter.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    struct stat src = {0};
    src.st_size = 2119380688;
    src.st_mode = S_IFREG | 0644;
    src.st_mtime = 1792851323;
    src.st_blocks = 169356;
    struct { uint64_t before; stat64_bionic st; uint64_t after; } out;
    memset(&out, 0xa5, sizeof(out));
    stat_newlib_to_bionic(&src, &out.st);
    int64_t size; uint32_t mode, mtime;
    memcpy(&size, (char *)&out.st + 48, 8);
    memcpy(&mode, (char *)&out.st + 16, 4);
    memcpy(&mtime, (char *)&out.st + 80, 4);
    assert(size == 2119380688 && mode == (uint32_t)src.st_mode && mtime == 1792851323);
    assert(out.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && out.after == out.before);
    for (unsigned i = 0; i < sizeof(out.st.__pad3); ++i) assert(out.st.__pad3[i] == 0);
    assert(out.st.__pad4 == 0 && out.st.st_mtim.tv_nsec == 0);
    src.st_size += (INT64_C(1) << 32);
    stat_newlib_to_bionic(&src, &out.st);
    memcpy(&size, (char *)&out.st + 48, 8);
    assert(size == (INT64_C(1) << 32) + 2119380688);
    puts("Android stat: native-consumed offsets, OBB/64-bit sizes, timestamps, padding and bounds passed");
}
