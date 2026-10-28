/* Gangstar-specific callbacks. Method identity includes class and descriptor. */
#include <falso_jni/FalsoJNI.h>
#include "utils/logger.h"
#include "utils/dialog.h"
#include "gangstar_trophies.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <so_util/so_util.h>
#include <psp2/io/stat.h>
#include <psp2/motion.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <time.h>
#include <psp2/kernel/threadmgr.h>
#include <pthread.h>
#include <stdatomic.h>
extern so_module so_mod;
static const char *installation_id(void) {
    static char value[48];
    if (*value) return value;
    FILE *f = fopen(DATA_PATH "installation-id.txt", "r");
    if (f) {
        int count = fscanf(f, "%47s", value); fclose(f);
        if (count == 1) return value;
    }
    snprintf(value, sizeof(value), "vita-%08lx%08lx", (unsigned long)time(NULL),
        (unsigned long)sceKernelGetProcessTimeWide());
    f = fopen(DATA_PATH "installation-id.txt", "w");
    if (!f) fatal_error("Gangstar: cannot save installation identifier");
    int ok = fprintf(f, "%s\n", value) > 0;
    if (fclose(f) || !ok) fatal_error("Gangstar: cannot persist installation identifier");
    return value;
}
static int accelerometer_enabled;
void gangstar_java_poll(void) {
    if (!accelerometer_enabled) return;
    SceMotionState state;
    if (sceMotionGetState(&state) < 0) return;
    void (*event)(JNIEnv *, jclass, jfloat, jfloat, jfloat) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_accelerometerEvent");
    // am.java converts Android m/s^2 to negative multiples of g before JNI.
    if (event) event(&jni, NULL, -state.acceleration.x,
        -state.acceleration.y, -state.acceleration.z);
}

static const char locale_class[] = "java/util/Locale";
static void (*delete_global_original)(JNIEnv *, jobject);
static jclass object_class(JNIEnv *env, jobject obj) {
    (void)env;
    if (obj == (jobject)locale_class) return (jclass)locale_class;
    fatal_error("Gangstar JNI: unknown object class %p", obj);
    return NULL;
}
static void delete_global(JNIEnv *env, jobject obj) {
    if (obj != (jobject)locale_class) delete_global_original(env, obj);
}
typedef struct { char *clazz, *name, *sig; atomic_uint calls; } Method;
static Method methods[256];
static atomic_uint method_count;
static int selected_language = -1;
static int menu_key_disabled = 1;
static atomic_int clear_frames = 2, no_swap_frames = 100;
static void keep_max(atomic_int *state, int value) {
    int old = atomic_load(state);
    while (value > old && !atomic_compare_exchange_weak(state, &old, value)) {}
}
static pthread_mutex_t method_lock = PTHREAD_MUTEX_INITIALIZER;

static jmethodID method_id(JNIEnv *env, jclass clazz, const char *name, const char *sig) {
    (void)env;
    if (!clazz || !name || !sig) fatal_error("Gangstar JNI: invalid method lookup");
    pthread_mutex_lock(&method_lock);
    for (unsigned i = 0; i < method_count; ++i)
        if (!strcmp(methods[i].clazz, (char *)clazz) && !strcmp(methods[i].name, name) &&
            !strcmp(methods[i].sig, sig)) {
            pthread_mutex_unlock(&method_lock); return (jmethodID)&methods[i];
        }
    if (method_count == 256) fatal_error("Gangstar JNI: method table full");
    Method *m = &methods[method_count++];
    m->clazz = strdup((char *)clazz); m->name = strdup(name); m->sig = strdup(sig);
    if (!m->clazz || !m->name || !m->sig) fatal_error("Gangstar JNI: method allocation failed");
    l_info("Gangstar JNI cache: %s.%s%s", m->clazz, name, sig);
    pthread_mutex_unlock(&method_lock);
    return (jmethodID)m;
}
static Method *called(jmethodID id) {
    Method *m = (Method *)id;
    if ((uintptr_t)m < (uintptr_t)methods || (uintptr_t)m >= (uintptr_t)(methods + method_count) ||
        ((uintptr_t)m - (uintptr_t)methods) % sizeof(Method))
        fatal_error("Gangstar JNI: invalid method ID %p", id);
    if (atomic_fetch_add(&m->calls, 1) < 2) l_info("Gangstar JNI call: %s.%s%s", m->clazz, m->name, m->sig);
    return m;
}
static void unsupported(Method *m) {
    fatal_error("Gangstar JNI: unsupported %s.%s%s", m->clazz, m->name, m->sig);
}
static jobject object_v(JNIEnv *env, jobject obj, jmethodID id, va_list args) {
    (void)obj; (void)args;
    Method *m = called(id);
    if (!strcmp(m->clazz, "java/util/Locale") && !strcmp(m->name, "getDefault"))
        return (jobject)locale_class;
    const char *value = NULL;
    if (!strcmp(m->name, "getPhoneModel")) value = "PS Vita";
    else if (!strcmp(m->name, "getPhoneManufacturer")) value = "Sony";
    else if (!strcmp(m->name, "getPhoneFW")) value = "4.0.4";
    else if (!strcmp(m->name, "getLocaleCountry")) value = "US";
    else if (!strcmp(m->name, "getVersion")) value = "1.2.1g";
    else if (!strcmp(m->name, "getSDFolder")) value = DATA_PATH;
    else if (!strcmp(m->name, "getSaveFolder")) value = DATA_PATH "saves/";
    else if (!strcmp(m->name, "getUserAgent")) value = "GangstarRio/1.2.1g (PS Vita)";
    else if (!strcmp(m->name, "getIdentifier")) value = installation_id();
    else if (!strcmp(m->name, "getAndroidID")) value = installation_id();
    // Device.java persists a generated vendor identifier for tracking/save
    // metadata. The offline Vita port uses its own persisted installation ID.
    else if (!strcmp(m->name, "getHDIDFV")) value = installation_id();
    else if (!strcmp(m->name, "getHDIDFVVersion")) value = "";
    else if (!strcmp(m->name, "getMac")) value = "00:00:00:00:00:00"; // Java's unavailable-WiFi fallback.
    else if (!strcmp(m->name, "getSerial") || !strcmp(m->name, "getSerialNo") ||
        !strcmp(m->name, "getCPUSerial")) return NULL;
    else if (!strcmp(m->name, "getPackage")) value = "com.gameloft.android.ANMP.GloftG4HM";
    else if (!strcmp(m->clazz, "java/util/Locale") && !strcmp(m->name, "getLanguage")) value = "en";
    else if (!strcmp(m->clazz, "java/util/Locale") && !strcmp(m->name, "getCountry")) value = "US";
    else if (!strcmp(m->clazz, "java/util/Locale") && !strcmp(m->name, "getISO3Language")) value = "eng";
    else if (!strcmp(m->clazz, "java/util/Locale") && !strcmp(m->name, "getISO3Country")) value = "USA";
    else if (!strcmp(m->clazz, "java/util/Locale") && !strcmp(m->name, "toString")) value = "en_US";
    else if (!strcmp(m->name, "getPreferenceString") &&
        !strcmp(m->sig, "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;")) {
        jstring key = va_arg(args, jstring);
        const char *name = (*env)->GetStringUTFChars(env, key, NULL);
        value = name && !strcmp(name, "SDFolder") ? DATA_PATH : "";
        if (name) (*env)->ReleaseStringUTFChars(env, key, (char *)name);
    }
    else if (!strcmp(m->name, "getObbFileName")) value = DATA_PATH "main.12030.com.gameloft.android.ANMP.GloftG4HM.obb";
    else if (!strcmp(m->name, "getObbPatchFileName")) value = DATA_PATH "patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb";
    else if (!strcmp(m->name, "getIMEI")) value = ""; // No telephony on Vita.
    else unsupported(m);
    return (*env)->NewStringUTF(env, value);
}
static void void_v(JNIEnv *env, jobject obj, jmethodID id, va_list args) {
    (void)env; (void)obj; (void)args;
    Method *m = called(id);
    if (!strcmp(m->clazz, "com/gameloft/android/ANMP/GloftG4HM/GL2JNILib") &&
        !strcmp(m->name, "notifyTrophy") && !strcmp(m->sig, "(I)V")) {
        int trophy = va_arg(args, int);
        if (gangstar_trophy_notify(DATA_PATH "androidTrophy.dat", trophy))
            l_warn("Gangstar Android trophy flag could not be saved: %d", trophy);
        return;
    }
    if (!strcmp(m->name, "SetDoneAcceptPP")) {
        // Match GL2JNILib.SetDoneAcceptPP(): persist only when the game calls
        // this setter. Startup never creates this acceptance marker.
        SceIoStat st;
        if (sceIoGetstat(DATA_PATH "doneAcceptPP", &st) < 0 &&
            sceIoMkdir(DATA_PATH "doneAcceptPP", 0777) < 0)
            fatal_error("Gangstar: cannot save privacy preference");
        l_info("Gangstar privacy preference recorded by the game");
        return;
    }
    if (!strcmp(m->name, "GetGangster4ButtonSwitch")) {
        // h.java uses false if its optional promotional-server request fails.
        void (*set_button)(JNIEnv *, jclass, jboolean) = (void *)so_symbol(&so_mod,
            "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_nativeSetGangster4ButtonSwitch");
        if (!set_button) fatal_error("Gangstar: missing promotional-button setter");
        set_button(env, (jclass)obj, JNI_FALSE);
        l_info("Gangstar promotional button disabled in offline port mode");
        return;
    }
    if (!strcmp(m->name, "sendGameTracking")) {
        l_info("Gangstar analytics disabled in offline port mode"); return;
    }
    // These Java setters only retain maxima; this APK's renderer always
    // clears and steps unconditionally and does not consume either counter.
    if (!strcmp(m->name, "setClearFrames")) { keep_max(&clear_frames, va_arg(args, int)); return; }
    if (!strcmp(m->name, "setNoSwapFrames")) { keep_max(&no_swap_frames, va_arg(args, int)); return; }
    if (!strcmp(m->name, "nativeGetLangIngame")) {
        selected_language = va_arg(args, int);
        l_info("Gangstar selected language index: %d", selected_language);
        return;
    }
    if (!strcmp(m->name, "SetDisableKey")) {
        menu_key_disabled = va_arg(args, int); return;
    }
    if (!strcmp(m->name, "setDisableKey")) { menu_key_disabled = 0; return; }
    if (!strcmp(m->clazz, "android/os/Process") && !strcmp(m->name, "setThreadPriority")) {
        int nice = va_arg(args, int);
        int priority = 160 + nice * 2;
        if (priority < 80) priority = 80;
        if (priority > 191) priority = 191;
        int result = sceKernelChangeThreadPriority(0, priority);
        if (result < 0) l_warn("Gangstar thread priority unavailable: 0x%x", result);
        return;
    }
    // Android schedules an Activity/view here; main.c owns that lifecycle.
    if (!strcmp(m->name, "startGame") || !strcmp(m->name, "createView")) return;
    // Android Activity orientation requests have no effect on Vita's fixed
    // landscape display. These callbacks only set Java Activity flags.
    if (!strcmp(m->name, "SetAutoOrientationOn") || !strcmp(m->name, "SetAutoOrientationOff") ||
        !strcmp(m->name, "SetAndroidOrientation") || !strcmp(m->name, "setPSKeyEnable")) return;
    if (!strcmp(m->name, "enableAccelerometer")) {
        int enabled = va_arg(args, int);
        (void)va_arg(args, double); // Android ignores the requested rate too.
        if (enabled && !accelerometer_enabled) {
            int result = sceMotionStartSampling();
            if (result < 0) l_warn("Gangstar motion sampling unavailable: 0x%x", result);
            accelerometer_enabled = result >= 0;
        } else if (!enabled && accelerometer_enabled) {
            sceMotionStopSampling(); accelerometer_enabled = 0;
        }
        return;
    }
    if (!strcmp(m->name, "setupPaths")) {
        void (*set_paths)(JNIEnv *, jclass, jstring, jstring, jstring) = (void *)so_symbol(
            &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_setPaths");
        if (!set_paths) fatal_error("Gangstar: missing setPaths");
        sceIoMkdir(DATA_PATH "saves", 0777);
        sceIoMkdir(DATA_PATH "cache", 0777);
        jstring data = (*env)->NewStringUTF(env, DATA_PATH);
        jstring home = (*env)->NewStringUTF(env, DATA_PATH "saves/");
        jstring cache = (*env)->NewStringUTF(env, DATA_PATH "cache/");
        set_paths(env, (jclass)obj, data, home, cache);
        l_success("Gangstar data/home/cache paths configured");
        return;
    }
    // Surface settings are consumed by the Android EGL chooser. Vita owns EGL.
    if (!strcmp(m->name, "setViewSettings")) {
        int pixel = va_arg(args, int), depth = va_arg(args, int), stencil = va_arg(args, int);
        int csaa = va_arg(args, int), extra = va_arg(args, int);
        l_info("Gangstar surface settings: pixel=%d depth=%d stencil=%d CSAA=%d extra=%d",
            pixel,depth,stencil,csaa,extra);
        return;
    }
    unsupported(m);
}
static jint integer_v(JNIEnv *env, jobject obj, jmethodID id, va_list args) {
    (void)env; (void)obj; (void)args;
    Method *m = called(id);
    if (!strcmp(m->name, "isWifiAlive")) return 0;
    if (!strcmp(m->name, "IsPowerAConnected")) return 0;
    unsupported(m); return 0;
}
static jboolean boolean_v(JNIEnv *env, jobject obj, jmethodID id, va_list args) {
    (void)env; (void)obj; (void)args;
    Method *m = called(id);
    if (!strcmp(m->name, "CheckDoneAcceptPP")) {
        SceIoStat st;
        int accepted = sceIoGetstat(DATA_PATH "doneAcceptPP", &st) >= 0;
        if (atomic_load(&m->calls) <= 2)
            l_info("Gangstar privacy preference: accepted=%d", accepted);
        return accepted;
    }
    if (!strcmp(m->name, "isUserMusicPlaying") || !strcmp(m->name, "isZEUSDevice") ||
        !strcmp(m->name, "isSlideEnabled") || !strcmp(m->name, "isAndroidOrientationAvailable")) return JNI_FALSE;
    if (!strcmp(m->name, "setCurrentContext")) return va_arg(args, int) == 0;
    unsupported(m); return JNI_FALSE;
}
static jfloat real_v(JNIEnv *env, jobject obj, jmethodID id, va_list args) {
    (void)env; (void)obj; (void)args;
    Method *m = called(id);
    if (!strcmp(m->name, "GetMemoryInfo")) return 512.0f;
    unsupported(m); return 0;
}
#define WRAPPER(NAME, TYPE) \
static TYPE NAME(JNIEnv *env, jobject obj, jmethodID id, ...) { \
    va_list args; va_start(args,id); TYPE result = NAME##_v(env,obj,id,args); va_end(args); return result; }
WRAPPER(object, jobject)
WRAPPER(integer, jint)
WRAPPER(boolean, jboolean)
WRAPPER(real, jfloat)
static void void_call(JNIEnv *env, jobject obj, jmethodID id, ...) {
    va_list args; va_start(args,id); void_v(env,obj,id,args); va_end(args);
}
void gangstar_java_install(void) {
    struct JNINativeInterface *table = (struct JNINativeInterface *)jni;
    delete_global_original = table->DeleteGlobalRef;
    table->DeleteGlobalRef = delete_global;
    table->GetObjectClass = object_class;
    table->GetMethodID = method_id;
    table->GetStaticMethodID = method_id;
    table->CallStaticObjectMethod = object;
    table->CallStaticObjectMethodV = object_v;
    table->CallObjectMethod = object;
    table->CallObjectMethodV = object_v;
    table->CallStaticVoidMethod = void_call;
    table->CallStaticVoidMethodV = void_v;
    table->CallStaticIntMethod = integer;
    table->CallStaticIntMethodV = integer_v;
    table->CallStaticBooleanMethod = boolean;
    table->CallStaticBooleanMethodV = boolean_v;
    table->CallStaticFloatMethod = real;
    table->CallStaticFloatMethodV = real_v;
    l_info("Gangstar Java bridge installed");
}
