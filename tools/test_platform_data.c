#include "gangstar_platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    GangstarAndroidStatfs fs;
    memset(&fs, 0xff, sizeof(fs));
    gangstar_statfs_data(&fs, (uint64_t)8 << 40, (uint64_t)5 << 40, 4096);
    assert(fs.blocks * fs.frsize == (uint64_t)8 << 40);
    assert(fs.bavail * fs.frsize == (uint64_t)5 << 40);
    assert(fs.bfree == fs.bavail);
    assert(fs.flags == 0 && fs.spare[3] == 0 && fs.files == 0);
    gangstar_statfs_data(&fs, 16385, 8193, 0);
    assert(fs.frsize == 4096 && fs.blocks == 4 && fs.bavail == 2);
    gangstar_statfs_data(&fs, 0, 0, 32768);
    assert(fs.frsize == 32768 && fs.blocks == 0 && fs.bavail == 0);
    puts("Platform data: Android struct layout and 64-bit storage conversion passed");
}
