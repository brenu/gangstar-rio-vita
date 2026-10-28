#include "gangstar_dynlink.h"
#include <string.h>

int gangstar_library_kind(const char *path) {
    if (!path) return 0;
    const char *leaf = strrchr(path, '/');
    leaf = leaf ? leaf + 1 : path;
    static const char *platform[] = {"libGLESv1_CM.so", "libGLESv2.so", "libEGL.so",
        "libc.so", "libm.so", "libz.so", "liblog.so", "libdl.so", "libOpenSLES.so", "libstdc++.so"};
    for (unsigned i = 0; i < sizeof(platform) / sizeof(platform[0]); ++i)
        if (!strcmp(leaf, platform[i])) return 0;
    if (!strcmp(leaf, "libgl2jni.so") || !strcmp(leaf, "libandroid.so")) return 1;
    return -1;
}
