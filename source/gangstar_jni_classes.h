#ifndef GANGSTAR_JNI_CLASSES_H
#define GANGSTAR_JNI_CLASSES_H
#include <stddef.h>
#include <string.h>
/* Class definitions live for the process lifetime. Callers serialize access;
 * local/global references share these stable, allocation-free handles. */
typedef struct { char names[64][192]; unsigned count; } RioJniClasses;
static inline const char *rio_jni_class_intern(RioJniClasses *classes,const char *name) {
    if (!name || !*name || strnlen(name,192)==192) return NULL;
    for(unsigned i=0;i<classes->count;++i)
        if(!strcmp(classes->names[i],name))return classes->names[i];
    if(classes->count==64)return NULL;
    char *result=classes->names[classes->count++];
    strcpy(result,name); return result;
}
static inline int rio_jni_class_owned(const RioJniClasses *classes,const void *handle) {
    for(unsigned i=0;i<classes->count;++i)
        if(handle==classes->names[i])return 1;
    return 0;
}
#endif
