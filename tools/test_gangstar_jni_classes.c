#include "gangstar_jni_classes.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    RioJniClasses c={0};
    const char *name="com/gameloft/android/ANMP/GloftG4HM/GL2JNILib";
    const char *handle=rio_jni_class_intern(&c,name);
    assert(handle && handle!=name && !strcmp(handle,name));
    for(unsigned i=0;i<1000000;++i)assert(rio_jni_class_intern(&c,name)==handle);
    assert(c.count==1 && rio_jni_class_owned(&c,handle));
    assert(!rio_jni_class_owned(&c,name) && !rio_jni_class_owned(&c,handle+1));
    assert(!rio_jni_class_intern(&c,NULL) && !rio_jni_class_intern(&c,""));
    char too_long[193];memset(too_long,'x',192);too_long[192]=0;
    assert(!rio_jni_class_intern(&c,too_long));
    for(unsigned i=1;i<64;++i) {
        char next[32];snprintf(next,sizeof(next),"class/%u",i);
        assert(rio_jni_class_intern(&c,next));
    }
    assert(!rio_jni_class_intern(&c,"overflow") && c.count==64);
    assert(rio_jni_class_intern(&c,name)==handle);
    puts("JNI classes: million lookups reuse one stable handle; ownership and bounds passed");
}
