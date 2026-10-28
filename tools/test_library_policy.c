#include "gangstar_dynlink.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(gangstar_library_kind(NULL) == 0);
    const char *platform[]={"libEGL.so","/system/lib/libGLESv1_CM.so","libOpenSLES.so","libstdc++.so","libc.so","libm.so","libz.so","liblog.so","libdl.so"};
    for(unsigned i=0;i<sizeof(platform)/sizeof(*platform);++i) assert(gangstar_library_kind(platform[i])==0);
    assert(gangstar_library_kind("libgl2jni.so") == 1);
    assert(gangstar_library_kind("release/libandroid.so") == 1);
    assert(gangstar_library_kind("libgl2jni.so.bak") == -1);
    assert(gangstar_library_kind("libs3e_android.so") == -1);
    assert(gangstar_library_kind("libs3eDialog.so") == -1);
    assert(gangstar_library_kind("") == -1);
    puts("Gangstar dynamic libraries: Android platform, game aliases and unsupported libraries checked");
}
