#ifndef GANGSTAR_JNI_STRINGS_H
#define GANGSTAR_JNI_STRINGS_H
#include <stddef.h>
#include <stdint.h>
#include <falso_jni/jni.h>
typedef struct {size_t live,bytes,created,destroyed,globals,pins;} RioJniStringStats;
void gangstar_jni_strings_install(struct JNINativeInterface *,uintptr_t (*thread_id)(void));
jstring gangstar_jni_string_constant(const char *value);
void gangstar_jni_strings_frame_end(void);
RioJniStringStats gangstar_jni_strings_stats(void);
#endif
