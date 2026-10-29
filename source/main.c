#include "gangstar_jni_strings.h"
#include "gangstar_audio_poll.h"
/* Gangstar Rio diagnostic loader, derived from the local Vita SO loader. */
#include "utils/init.h"
#include "utils/logger.h"
#include "utils/dialog.h"
#include "reimpl/controls.h"
#include "utils/glutil.h"

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <psp2/kernel/processmgr.h>
#include <stdlib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <malloc.h>
#include "reimpl/io.h"
#include "gangstar_controls.h"
#include "gangstar_perf_policy.h"
#include "gangstar_archive_cache.h"
#include "gangstar_loose_index.h"
#include "gangstar_asset_perf.h"
#include "gangstar_wait_perf.h"
#include "gangstar_native_perf.h"
#include "gangstar_shader_warmup.h"

// The Vita thread reproduces the Activity and GLSurfaceView lifecycle.
int _newlib_heap_size_user = 256 * 1024 * 1024;
const SceUInt32 sceUserMainThreadStackSize = 8 * 1024 * 1024;

so_module so_mod;
static jclass gangstar_game_class;
void gangstar_java_install(void);
void gangstar_java_poll(void);
void gangstar_capture(unsigned frame);
int gangstar_sync_selftest(void);
void gangstar_io_report(void);
void gangstar_native_startup_frame(void);
void gangstar_native_reuse_frame(unsigned);
void gangstar_native_reuse_report(void);
void gangstar_native_startup_report(unsigned frame);
void gangstar_heap_diag_report(unsigned frame);
void gangstar_audio_report(void);
void gangstar_audio_output_report(void);

static int gangstar_main(void) {
    soloader_init_all();
    gangstar_archive_init();
#ifdef GANGSTAR_LOOSE_PATH_INDEX
    l_info("RIO_LOOSE_INDEX startup_absent=%u",rio_loose_index_init(DATA_PATH));
#endif
    if (gangstar_sync_selftest()) fatal_error("Vita synchronization self-test failed");
    gangstar_java_install();
    const char *archives[] = {
        DATA_PATH "main.12030.com.gameloft.android.ANMP.GloftG4HM.obb",
        DATA_PATH "patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb"
    };
    const int64_t archive_sizes[] = {INT64_C(2119380688), INT64_C(3709754)};
    for (unsigned i = 0; i < 2; ++i) {
        SceIoStat native;
        stat64_bionic android;
        if (sceIoGetstat(archives[i], &native) < 0 || native.st_size != archive_sizes[i] ||
            stat_soloader(archives[i], &android) || android.st_size != archive_sizes[i])
            fatal_error("Gangstar: OBB size checkpoint failed: %s", archives[i]);
        FILE *f = fopen_soloader(archives[i], "rb");
        if (!f) fatal_error("Gangstar: cannot open staged OBB: %s", archives[i]);
        unsigned char header[16];
        size_t count = fread_soloader(header, 1, sizeof(header), f);
        fclose_soloader(f);
        if (count != sizeof(header)) fatal_error("Gangstar: cannot read OBB header: %s", archives[i]);
        l_success("Gangstar OBB verified: size=%lld at Android stat offset 48; header readable: %s",
            (long long)android.st_size, archives[i]);
    }

    jint (*on_load)(JavaVM *, void *) =
        (void *)so_symbol(&so_mod, "JNI_OnLoad");
    if (!on_load) {
        fatal_error("Gangstar diagnostic: libgl2jni.so has no JNI_OnLoad export");
        return 1;
    }

    l_info("Calling Gangstar JNI_OnLoad with the Vita Java bridge");
    jint version = on_load(&jvm, NULL);
    l_info("Gangstar JNI_OnLoad returned 0x%x", version);
    if (version < JNI_VERSION_1_2 || version > JNI_VERSION_1_6) {
        fatal_error("Gangstar diagnostic: unexpected JNI version 0x%x", version);
        return 1;
    }

    l_success("Gangstar native library loaded and JNI_OnLoad passed");
    // On Android, InAppBilling.init(Activity) also initializes the shared
    // AndroidOS VM/TLS bridge. The game later uses that bridge for platform
    // queries even without purchases. Initialize the platform layer directly.
    void (*platform_on_load)(JavaVM *) = (void *)so_symbol(&so_mod, "AndroidOS_JNIOnload");
    if (!platform_on_load) fatal_error("Gangstar: missing AndroidOS_JNIOnload");
    l_info("Initializing Gangstar AndroidOS VM/TLS bridge");
    platform_on_load(&jvm);
    l_success("Gangstar AndroidOS platform bridge initialized");

    jclass installer_class = (*jni).FindClass(
        &jni, "com/gameloft/android/ANMP/GloftG4HM/installer/GameInstaller");
    jclass game_class = (*jni).FindClass(
        &jni, "com/gameloft/android/ANMP/GloftG4HM/GL2JNILib");
    gangstar_game_class = game_class;
    if (!installer_class || !game_class) {
        fatal_error("Gangstar startup: could not create Java class handles");
        return 1;
    }

    l_info("Calling GameInstaller.initNative to reproduce Android startup ordering");
    void (*installer_init)(JNIEnv *, jclass) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_installer_GameInstaller_initNative");
    if (!installer_init) {
        fatal_error("Gangstar startup: missing GameInstaller.initNative export");
        return 1;
    }
    installer_init(&jni, installer_class);

    // Android calls GameInstaller.getPublicKey() before constructing the DRM
    // policy. Despite its name, the native method initializes lockPointer1,
    // which the policy's getConstString/setConst routines require.
    void (*installer_get_public_key)(JNIEnv *, jobject) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_installer_GameInstaller_getPublicKey");
    if (!installer_get_public_key) {
        fatal_error("Gangstar startup: missing GameInstaller.getPublicKey export");
        return 1;
    }
    l_info("Calling GameInstaller.getPublicKey to initialize the DRM key state");
    installer_get_public_key(&jni, installer_class);

    // The retired Android licensing service cannot populate this installer
    // latch on Vita. This is an explicit offline port startup path, not a
    // successful response from the original licensing service.
    int **installer_latch = (int **)so_symbol(&so_mod, "lockPointer3");
    if (!installer_latch) {
        fatal_error("Gangstar startup: missing installer latch");
        return 1;
    }
    if (!*installer_latch) {
        *installer_latch = malloc(sizeof(**installer_latch));
        if (!*installer_latch) return 1;
        **installer_latch = 1;
    }
    l_warn("Offline Vita startup: initializing installer latch; no Android license service");
    void (*installer_start)(JNIEnv *, jclass) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_installer_GameInstaller_nativeStart");
    if (!installer_start) return 1;
    l_info("Calling GameInstaller.nativeStart");
    installer_start(&jni, installer_class);
    void **lock_pointer4 = (void **)so_symbol(&so_mod, "lockPointer4");
    if (!lock_pointer4 || !*lock_pointer4) {
        fatal_error("Gangstar startup: nativeStart did not initialize lockPointer4");
        return 1;
    }
    l_success("Installer nativeStart initialized lockPointer4");

    void (*game_init)(JNIEnv *, jclass) =
        (void *)so_symbol(&so_mod,
            "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_init");
    if (!game_init) {
        fatal_error("Gangstar init: missing GL2JNILib.init export");
        return 1;
    }

    l_info("Calling GL2JNILib.init after installer startup");
    game_init(&jni, game_class);
    l_success("GL2JNILib.init returned");
    void (*set_paths)(JNIEnv *, jclass, jstring, jstring, jstring) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_setPaths");
    void (*init_gl)(JNIEnv *, jclass) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_initGL");
    void (*view_settings)(JNIEnv *, jclass) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_getViewSettings");
    void (*state_changed)(JNIEnv *, jclass, jboolean) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_stateChanged");
    void (*resize)(JNIEnv *, jclass, jint, jint) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_resize");
    void (*step)(JNIEnv *, jclass) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_step");
    if (!set_paths || !init_gl || !view_settings || !state_changed || !resize || !step)
        fatal_error("Gangstar: missing renderer JNI exports");
    gl_init();
    gangstar_shader_warmup();
    l_info("Calling Gangstar initGL and getViewSettings");
    init_gl(&jni, game_class);
    view_settings(&jni, game_class);
    // m.java reports how many shared EGL contexts actually got created.
    // vitaGL supplies one context, so request the engine's zero-extra fallback.
    void (*set_extra_contexts)(JNIEnv *, jclass, jint) = (void *)so_symbol(
        &so_mod, "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_setNumExtraContext");
    if (!set_extra_contexts) fatal_error("Gangstar: missing setNumExtraContext");
    set_extra_contexts(&jni, game_class, 0);
    l_info("Gangstar graphics: one Vita context, zero extra EGL contexts");
    l_info("Calling Gangstar stateChanged(true) and resize(960,544)");
    state_changed(&jni, game_class, JNI_TRUE);
    resize(&jni, game_class, 960, 544);
    l_info("Entering Gangstar frame loop");
    GangstarPerfWindow performance = {0};
    struct { unsigned frame; uint64_t total, read, upload, cache, etc_free, etc_alloc, etc_swizzle, etc_setup; unsigned etc_calls, etc_reserved, etc_reused, etc_fallback, etc_migrated, etc_no_room, upload_width, upload_height;
        GangstarNativePerf native; GangstarWaitPerf waits; GangstarWaitEvent longest[3]; } hitches[64];
    unsigned hitch_count = 0, hitch_dropped = 0;
    unsigned slow_frames = 0, very_slow_frames = 0;
    uint64_t last_swap_end=0, outside_us=0, outside_worst=0, last_report_us=0;
    SceUID game_thread = sceKernelGetThreadId();
    for (unsigned frame = 0; ; ++frame) {
        uint64_t frame_begin = sceKernelGetProcessTimeWide();
        if (last_swap_end && frame_begin>=last_swap_end) {
            uint64_t gap=frame_begin-last_swap_end;
            outside_us+=gap; if (gap>outside_worst) outside_worst=gap;
        }
        SceKernelThreadInfo thread_before = {.size = sizeof(thread_before)};
        int thread_sample = frame < 32 ? sceKernelGetThreadInfo(game_thread, &thread_before) : -1;
        gangstar_io_frame_begin();
        gangstar_asset_frame();
        gangstar_wait_frame();
        gangstar_native_startup_frame();
        gangstar_native_reuse_frame(frame);
        gangstar_archive_frame(frame);
        gangstar_hud_trace_poll();
        gangstar_java_poll();
        if (frame) controls_poll();
        gangstar_controls_poll();
        // q.java resets alpha as well as RGB before every native frame.
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        uint64_t step_begin = sceKernelGetProcessTimeWide();
        step(&jni, game_class);
        gangstar_jni_strings_frame_end();

        uint64_t swap_begin = sceKernelGetProcessTimeWide();
        gl_swap();
        uint64_t swap_end = sceKernelGetProcessTimeWide();
        last_swap_end=swap_end;
        GangstarFrameIo frame_io = gangstar_io_frame_end();
        if (swap_end - frame_begin >= 50000) {
            if (hitch_count < 64) {
                GangstarAssetPerf timing = gangstar_asset_stats();
                GangstarArchiveFrame io_timing = gangstar_archive_frame_stats();
                unsigned i = hitch_count++;
                hitches[i].frame = frame; hitches[i].total = swap_end - frame_begin;
                hitches[i].read = frame_io.read_us; hitches[i].upload = timing.us[RIO_ASSET_TEXTURE];
                hitches[i].cache = io_timing.elapsed_us;
                hitches[i].etc_free=timing.us[RIO_ASSET_ETC_FREE]; hitches[i].etc_alloc=timing.us[RIO_ASSET_ETC_ALLOC];
                hitches[i].etc_swizzle=timing.us[RIO_ASSET_ETC_SWIZZLE]; hitches[i].etc_setup=timing.us[RIO_ASSET_ETC_SETUP];
                hitches[i].etc_calls=timing.calls[RIO_ASSET_ETC_SWIZZLE];
                hitches[i].etc_reserved=timing.calls[RIO_ASSET_ETC_RESERVE];
                hitches[i].etc_reused=timing.calls[RIO_ASSET_ETC_REUSE];
                hitches[i].etc_fallback=timing.calls[RIO_ASSET_ETC_FALLBACK];
                hitches[i].etc_migrated=timing.calls[RIO_ASSET_ETC_MIGRATED];
                hitches[i].etc_no_room=timing.calls[RIO_ASSET_ETC_NO_ROOM];
                hitches[i].upload_width=timing.max_width; hitches[i].upload_height=timing.max_height;
                hitches[i].native = (GangstarNativePerf){0}; hitches[i].waits = (GangstarWaitPerf){0};
                if (hitches[i].total>=100000) {
                    hitches[i].native=gangstar_native_stats(); hitches[i].waits=gangstar_wait_stats();
                    hitches[i].longest[0]=gangstar_wait_longest(RIO_WAIT_MUTEX);
                    hitches[i].longest[1]=gangstar_wait_longest(RIO_WAIT_COND);
                    hitches[i].longest[2]=gangstar_wait_longest(RIO_WAIT_FILE);
                }
            } else ++hitch_dropped;
        }
        // One completed frame, rather than one isolated read or a five-second
        // average. Log only severe stalls to avoid per-frame file traffic.
        if (swap_end - frame_begin >= 250000) {
            if (!thread_sample) {
                SceKernelThreadInfo after = {.size = sizeof(after)};
                if (!sceKernelGetThreadInfo(game_thread, &after))
                    l_info("RIO_THREAD_RUN frame=%u wall_us=%llu run_clocks=%llu releases=%u preemptions=%u",
                        frame, (unsigned long long)(swap_end - frame_begin),
                        (unsigned long long)(after.runClocks - thread_before.runClocks),
                        after.threadReleaseCount - thread_before.threadReleaseCount,
                        after.threadPreemptCount - thread_before.threadPreemptCount);
            }
            gangstar_wait_report(frame);
            gangstar_native_startup_report(frame);
            GangstarArchiveFrame archive_io = gangstar_archive_frame_stats();
            GangstarAssetPerf asset = gangstar_asset_stats();
            l_info("RIO_ASSET frame=%u texture_calls=%u texture_us=%llu compressed_bytes=%llu zlib_calls=%u zlib_us=%llu zlib_out=%llu shader_calls=%u shader_us=%llu",
                frame, asset.calls[RIO_ASSET_TEXTURE], (unsigned long long)asset.us[RIO_ASSET_TEXTURE],
                (unsigned long long)asset.bytes[RIO_ASSET_TEXTURE], asset.calls[RIO_ASSET_ZLIB],
                (unsigned long long)asset.us[RIO_ASSET_ZLIB], (unsigned long long)asset.bytes[RIO_ASSET_ZLIB],
                asset.calls[RIO_ASSET_SHADER], (unsigned long long)asset.us[RIO_ASSET_SHADER]);
            l_info("RIO_MIP frame=%u base_uploads=%u mip_uploads=%u generations=%u max_width=%u max_height=%u",
                frame, asset.base_uploads, asset.mip_uploads, asset.mip_generations, asset.max_width, asset.max_height);
            l_info("RIO_STALL frame=%u at_us=%llu total_us=%llu prep_us=%llu step_us=%llu swap_us=%llu main_read_us=%llu main_read_calls=%u main_read_bytes=%llu",
                frame, (unsigned long long)frame_begin,
                (unsigned long long)(swap_end - frame_begin),
                (unsigned long long)(step_begin - frame_begin),
                (unsigned long long)(swap_begin - step_begin),
                (unsigned long long)(swap_end - swap_begin),
                (unsigned long long)frame_io.read_us, frame_io.calls,
                (unsigned long long)frame_io.bytes);
            l_info("RIO_STALL_IO frame=%u calls=%u requested=%llu returned=%llu cache_path_us=%llu disk_calls=%u disk_bytes=%llu disk_us=%llu",
                frame, archive_io.calls, (unsigned long long)archive_io.requested,
                (unsigned long long)archive_io.returned,
                (unsigned long long)archive_io.elapsed_us, archive_io.disk_calls,
                (unsigned long long)archive_io.disk_bytes,
                (unsigned long long)archive_io.disk_us);
        }
        uint64_t previous_end = performance.last_end;
        if (previous_end && swap_end >= previous_end) {
            uint64_t duration = swap_end - previous_end;
            slow_frames += duration > 50000;
            very_slow_frames += duration > 100000;
        }
        GangstarPerfWindow sample;
        if (gangstar_perf_add(&performance, swap_begin, swap_end, &sample)) {
            uint64_t report_begin=sceKernelGetProcessTimeWide();
            unsigned fps = gangstar_perf_fps10(&sample);
            l_info("RIO_PERF frames=%u fps=%u.%u work_us=%u swap_us=%u worst_us=%u over50ms=%u over100ms=%u outside_us=%llu outside_worst_us=%llu previous_report_us=%llu",
                (unsigned)sample.frames, fps/10, fps%10,
                (unsigned)(sample.work/sample.frames), (unsigned)(sample.swap/sample.frames),
                (unsigned)sample.worst, slow_frames, very_slow_frames,
                (unsigned long long)outside_us,(unsigned long long)outside_worst,(unsigned long long)last_report_us);
            outside_us=outside_worst=0;
            slow_frames = very_slow_frames = 0;
            for (unsigned i = 0; i < hitch_count; ++i)
                l_info("RIO_HITCH frame=%u total_us=%llu read_us=%llu texture_us=%llu cache_path_us=%llu",
                    hitches[i].frame, (unsigned long long)hitches[i].total, (unsigned long long)hitches[i].read,
                    (unsigned long long)hitches[i].upload, (unsigned long long)hitches[i].cache);
            for (unsigned i=0;i<hitch_count;++i) if (hitches[i].total>=100000) {
                GangstarNativePerf n=hitches[i].native; GangstarWaitPerf w=hitches[i].waits;
                l_info("RIO_HITCH_UPLOAD frame=%u texture_us=%llu etc_levels=%u retire_us=%llu allocation_us=%llu swizzle_us=%llu setup_us=%llu reserved=%u reused=%u fallback=%u migrated=%u no_room=%u max_width=%u max_height=%u",
                    hitches[i].frame,(unsigned long long)hitches[i].upload,hitches[i].etc_calls,
                    (unsigned long long)hitches[i].etc_free,(unsigned long long)hitches[i].etc_alloc,
                    (unsigned long long)hitches[i].etc_swizzle,(unsigned long long)hitches[i].etc_setup,
                    hitches[i].etc_reserved,hitches[i].etc_reused,hitches[i].etc_fallback,hitches[i].etc_migrated,
                    hitches[i].etc_no_room,hitches[i].upload_width,hitches[i].upload_height);
                l_info("RIO_HITCH_CPU frame=%u top=%s top_us=%llu calls=%u lookup_us=%llu dds_us=%llu animation_us=%llu resource_us=%llu world_us=%llu graphics_us=%llu stream_us=%llu file_load_us=%llu wait_file_us=%llu mutex_us=%llu cond_us=%llu sem_us=%llu sleep_us=%llu",
                    hitches[i].frame,n.top?n.top:"none",(unsigned long long)n.top_us,n.top_calls,
                    (unsigned long long)n.lookup,(unsigned long long)n.dds,(unsigned long long)n.animation,
                    (unsigned long long)n.resources,(unsigned long long)n.world,(unsigned long long)n.graphics,
                    (unsigned long long)n.streaming,(unsigned long long)n.file_load,
                    (unsigned long long)w.us[RIO_WAIT_FILE],(unsigned long long)w.us[RIO_WAIT_MUTEX],
                    (unsigned long long)w.us[RIO_WAIT_COND],(unsigned long long)w.us[RIO_WAIT_SEM],
                    (unsigned long long)w.us[RIO_WAIT_SLEEP]);
                for (unsigned j=0;j<3;++j) {
                    GangstarWaitEvent e=hitches[i].longest[j];
                    if (e.us>=5000) l_info("RIO_HITCH_WAIT frame=%u kind=%u op=%s elapsed_us=%llu caller=0x%x object=0x%x result=%d",
                        hitches[i].frame,e.kind,e.operation?e.operation:"unknown",(unsigned long long)e.us,
                        (unsigned)e.caller,(unsigned)e.object,e.result);
                    if(e.us>=5000 && e.kind==RIO_WAIT_COND)
                        l_info("RIO_HITCH_COND frame=%u setup_us=%llu before_signal_us=%llu resume_us=%llu cleanup_us=%llu relock_us=%llu signaled=%d signal_tid=%d",
                            hitches[i].frame,(unsigned long long)e.condition.setup_us,
                            (unsigned long long)e.condition.before_signal_us,(unsigned long long)e.condition.resume_us,
                            (unsigned long long)e.condition.cleanup_us,(unsigned long long)e.condition.relock_us,
                            e.condition.signaled,e.condition.signal_thread);
                    if(e.us>=5000 && e.path[0])
                        l_info("RIO_HITCH_PATH frame=%u op=%s elapsed_us=%llu result=%d errno=%d path=%s",
                            hitches[i].frame,e.operation,(unsigned long long)e.us,e.result,e.saved_errno,e.path);
                    if(e.us>=5000 && e.observed_owner>0) {
                        /* Snapshot was taken before acquisition, not a complete owner history.
                           Query only at report time, outside the measured game frame. */
                        SceKernelThreadInfo owner={0};owner.size=sizeof(owner);
                        int found=sceKernelGetThreadInfo(e.observed_owner,&owner);
                        l_info("RIO_HITCH_OWNER frame=%u observed_tid=%d info_result=%d name=%s priority=%d acquire_caller=0x%x",
                            hitches[i].frame,e.observed_owner,found,found<0?"unavailable":owner.name,
                            found<0?-1:owner.currentPriority,(unsigned)e.owner_acquire_site);
                    }
                }
            }
            if (hitch_dropped) l_info("RIO_HITCH dropped=%u", hitch_dropped);
            hitch_count = hitch_dropped = 0;
            gangstar_io_report();
            gangstar_heap_diag_report(frame);
            RioJniStringStats strings=gangstar_jni_strings_stats();
            l_info("RIO_JNI_STRINGS live=%u bytes=%u created=%u destroyed=%u globals=%u pins=%u",
                (unsigned)strings.live,(unsigned)strings.bytes,(unsigned)strings.created,
                (unsigned)strings.destroyed,(unsigned)strings.globals,(unsigned)strings.pins);
            gangstar_archive_report();
            gangstar_native_reuse_report();
            gangstar_audio_report();
            gangstar_audio_output_report();
            gangstar_upload_report();
            gangstar_audio_poll_report();
            gangstar_log_report();
            last_report_us=sceKernelGetProcessTimeWide()-report_begin;
        }
        if (frame == 0 || frame == 29 || frame == 299) {
            GLenum error = glGetError();
            l_success("Gangstar rendered frame %u; GL error=0x%x", frame + 1, error);
            struct mallinfo memory = mallinfo();
            l_info("Gangstar heap at frame %u: used=%u free=%u arena=%u",
                frame + 1, (unsigned)memory.uordblks,
                (unsigned)memory.fordblks, (unsigned)memory.arena);
        }
        sceKernelPowerTick(0);
    }
    return 0;
}

int main(void) {
    gangstar_log_start();
    l_info("Gangstar Rio diagnostic starting; title ID G4HM00001");
    l_info("Gangstar VitaGL flags: %s", GANGSTAR_VITAGL_BUILD_FLAGS);
    int result = gangstar_main();
    gangstar_log_finish();
    sceKernelExitProcess(result);
    return result;
}

void controls_handler_key(int32_t keycode, ControlsAction action) {
    if (gangstar_controls_filter_key(keycode,action)) return;
    static void (*event)(JNIEnv *, jclass, jint, jboolean);
    if (!event) event = (void *)so_symbol(&so_mod,
        "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_keyEvent");
    if (event) event(&jni, gangstar_game_class, keycode, action == CONTROLS_ACTION_DOWN);
}

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    static int fingers[6] = {-1, -1, -1, -1, -1, -1};
    int slot = -1;
    for (int i = 0; i < 6; ++i) if (fingers[i] == id) slot = i;
    if (slot < 0 && action == CONTROLS_ACTION_DOWN) {
        for (int i = 0; i < 6; ++i) if (fingers[i] < 0) { slot = i; fingers[i] = id; break; }
    }
    if (slot < 0) return;
    static void (*event)(JNIEnv *, jclass, jint, jint, jint, jint);
    if (!event) event = (void *)so_symbol(&so_mod,
        "Java_com_gameloft_android_ANMP_GloftG4HM_GL2JNILib_touchEvent");
    // GloftG4HM.onTouchEvent passes (0=up/1=down/2=move, x, y, pointerID).
    if (event) event(&jni, gangstar_game_class, action, (int)x, (int)y, slot);
    if (action == CONTROLS_ACTION_UP) fingers[slot] = -1;
}

void controls_handler_analog(ControlsStickId which, float x, float y, ControlsAction action) {
    (void)action;
    gangstar_controls_stick(which, x, y);
}
