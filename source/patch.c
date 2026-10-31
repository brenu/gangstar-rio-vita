/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>

#ifdef __cplusplus
extern "C"
{
#endif
	extern so_module so_mod;
#ifdef __cplusplus
};
#endif

#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RX (0x0C20D050)

#include "utils/logger.h"
#include "utils/dialog.h"
#include "reimpl/sys.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include "gangstar_controls.h"

void __kuser_memory_barrier(void) {
	__sync_synchronize();
}

static bool atomic_block_ready;

void kuser_patch(so_module *module) {
	if (!atomic_block_ready) {
	SceKernelAllocMemBlockKernelOpt opt;
	memset(&opt, 0, sizeof(SceKernelAllocMemBlockKernelOpt));
	opt.size = sizeof(SceKernelAllocMemBlockKernelOpt);
	opt.attr = 0x1;
	opt.field_C = (SceUInt32)0x9A000000;
	if (kuKernelAllocMemBlock("atomic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RX, 0x1000, &opt) < 0)
		fatal_error("Error could not allocate atomic block.");
	if (kuKernelMemProtect((void *)0x9A000000, (SceSize)0x1000, KU_KERNEL_PROT_EXEC | KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE) < 0)
		fatal_error("Error: could not protect the atomic helper block.");

	hook_addr(0x9A000FA0, (uintptr_t)__kuser_memory_barrier);
	hook_addr(0x9A000FC0, (uintptr_t)__atomic_cmpxchg);
	atomic_block_ready = true;
	}

	uint32_t patched_addr;
	for (uint32_t addr = module->text_base; addr + 4 <= module->text_base + module->text_size; addr += 4) {
		uint32_t *a = (uint32_t *)addr;
		if (*a == 0xFFFF0FC0) {
			l_debug("Patching 0x%x -> __kuser_cmpxchg", a);
			patched_addr = 0x9A000FC0;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
		}
		else if (*a == 0xFFFF0FA0) {
			l_debug("Patching 0x%x -> __kuser_memory_barrier", a);
			patched_addr = 0x9A000FA0;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
		}
	}
}

void so_patch_module(so_module *module) {
	kuser_patch(module);
}

static void gangstar_console_raw(const char *text) {
    if (text && *text) l_info("[Gangstar] %s", text);
}

static so_hook gangstar_profile_hook;
static unsigned char **gangstar_phone_profile;
static int (*gangstar_viewport_width)(void);
static int (*gangstar_viewport_height)(void);

static void gangstar_customize_profile(void) {
    // Preserve Android's special-device adjustments before adding the Vita.
    // This is the same restore/call/reapply convention as SO_CONTINUE, with
    // a void return type. Profile selection runs on the initialization thread.
    so_hook *hook = &gangstar_profile_hook;
    sceClibMemcpy((void *)hook->addr, hook->orig_instr, sizeof(hook->orig_instr));
    kuKernelFlushCaches((void *)hook->addr, sizeof(hook->orig_instr));
    ((void (*)(void))hook->addr)();
    sceClibMemcpy((void *)hook->addr, hook->patch_instr, sizeof(hook->patch_instr));
    kuKernelFlushCaches((void *)hook->addr, sizeof(hook->patch_instr));

    unsigned char *profile = *gangstar_phone_profile;
    if (!profile) fatal_error("Gangstar: missing selected phone profile");
    // Verified PerfProfile::Read layout in the supplied 1.2.1g library.
    const char *name;
    float ratio;
    memcpy(&name, profile + 0x30, sizeof(name));
    memcpy(&ratio, profile + 0x70, sizeof(ratio));
    int width = gangstar_viewport_width(), height = gangstar_viewport_height();
    float previous = ratio;
    if (width == 960 && height == 544 && name &&
        !strcmp(name, "DefaultProfile") && ratio == 1.0f) {
        // The resource selector accepts 544 as androidWVGA540, but the phone
        // profiles require an exact 540-pixel short edge. The unmatched
        // DefaultProfile then overrides hudImageRatio to 1 for wide screens.
        // Its atlas is still the 0.8-scale WVGA540 pair. Restore that atlas's
        // ratio before ASprite construction and gameswf texture queries.
        ratio = 0.8f;
        memcpy(profile + 0x70, &ratio, sizeof(ratio));
    }
    l_info("Gangstar Vita HUD profile: %s viewport=%dx%d hudImageRatio=%g -> %g",
        name ? name : "(null)", width, height, previous, ratio);
}

static void gangstar_patch_hud_profile(void) {
    uintptr_t customize = so_symbol(&so_mod, "_Z30customProfileForSpecialDevicesv");
    gangstar_phone_profile = (void *)so_symbol(&so_mod, "gPhonePerf");
    gangstar_viewport_width = (void *)so_symbol(&so_mod, "_ZN11Application16GetViewportWidthEv");
    gangstar_viewport_height = (void *)so_symbol(&so_mod, "_ZN11Application17GetViewportHeightEv");
    static const uint32_t expected[] = {0xe59f3030, 0xe79f3003};
    if (!customize || !gangstar_phone_profile || !gangstar_viewport_width ||
        !gangstar_viewport_height || memcmp((void *)customize, expected, sizeof(expected)))
        fatal_error("Gangstar: unsupported HUD profile customization layout");
    gangstar_profile_hook = hook_addr(customize, (uintptr_t)gangstar_customize_profile);
}

static so_hook gangstar_audio_hook;
static void (*gangstar_vox_lock)(void *), (*gangstar_vox_unlock)(void *);
static void (*gangstar_vox_fill)(void *);
static atomic_uint audio_callbacks, audio_nonzero, audio_peak, audio_worst_us;
static atomic_uint audio_entered, audio_locked, audio_filled;

static void gangstar_audio_callback(const void *queue, void *driver) {
    (void)queue;
    if (!driver) return;
    atomic_fetch_add_explicit(&audio_entered, 1, memory_order_relaxed);
    uint64_t started = sceKernelGetProcessTimeWide();
    // Exact native CallbackOSL sequence: lock driver mutex, refill, unlock.
    // Inspect the mixed PCM under that same lock; never log from this thread.
    gangstar_vox_lock((unsigned char *)driver + 4);
    atomic_fetch_add_explicit(&audio_locked, 1, memory_order_relaxed);
    gangstar_vox_fill(driver);
    atomic_fetch_add_explicit(&audio_filled, 1, memory_order_relaxed);
    const int16_t *pcm;
    uint32_t bytes;
    memcpy(&pcm, (unsigned char *)driver + 0xe4, sizeof(pcm));
    memcpy(&bytes, (unsigned char *)driver + 0xe8, sizeof(bytes));
    unsigned peak = 0;
    if (pcm && bytes <= 64 * 1024) {
        for (unsigned i = 0; i < bytes / sizeof(*pcm); ++i) {
            int value = pcm[i];
            unsigned magnitude = value < 0 ? (unsigned)-value : (unsigned)value;
            if (magnitude > peak) peak = magnitude;
        }
    }
    gangstar_vox_unlock((unsigned char *)driver + 4);
    atomic_fetch_add(&audio_callbacks, 1);
    if (peak) atomic_fetch_add(&audio_nonzero, 1);
    unsigned old = atomic_load(&audio_peak);
    while (peak > old && !atomic_compare_exchange_weak(&audio_peak, &old, peak)) {}
    unsigned elapsed = (unsigned)(sceKernelGetProcessTimeWide() - started);
    old = atomic_load(&audio_worst_us);
    while (elapsed > old && !atomic_compare_exchange_weak(&audio_worst_us, &old, elapsed)) {}
}

void gangstar_audio_report(void) {
    l_info("RIO_AUDIO_STAGES entered=%u locked=%u filled=%u",
        atomic_exchange(&audio_entered, 0), atomic_exchange(&audio_locked, 0),
        atomic_exchange(&audio_filled, 0));
    l_info("RIO_AUDIO_PCM callbacks=%u nonzero=%u peak=%u worst_callback_us=%u",
        atomic_exchange(&audio_callbacks, 0), atomic_exchange(&audio_nonzero, 0),
        atomic_exchange(&audio_peak, 0), atomic_exchange(&audio_worst_us, 0));
}
static void gangstar_audio_initialize(void *driver, void *settings) {
    so_hook *hook = &gangstar_audio_hook;
    sceClibMemcpy((void *)hook->addr, hook->orig_instr, sizeof(hook->orig_instr));
    kuKernelFlushCaches((void *)hook->addr, sizeof(hook->orig_instr));
    ((void (*)(void *, void *))hook->addr)(driver, settings);
    sceClibMemcpy((void *)hook->addr, hook->patch_instr, sizeof(hook->patch_instr));
    kuKernelFlushCaches((void *)hook->addr, sizeof(hook->patch_instr));
    // Read-only checkpoint: 0=uninitialized, 1=AudioTrack, 2=OpenSL.
    unsigned char *state = driver;
    uint32_t mode, engine, player, queue, pcm, bytes;
    memcpy(&mode, state + 0xa0, 4);
    memcpy(&engine, state + 0xcc, 4); memcpy(&player, state + 0xd8, 4);
    memcpy(&queue, state + 0xe0, 4); memcpy(&pcm, state + 0xe4, 4);
    memcpy(&bytes, state + 0xe8, 4);
    l_info("RIO_AUDIO driver=%p mode=%u engine=0x%x player=0x%x queue=0x%x pcm=0x%x bytes=%u",
        driver, mode, engine, player, queue, pcm, bytes);
}

static void gangstar_trace_audio_init(void) {
    uintptr_t init = so_symbol(&so_mod, "_ZN3vox13DriverAndroid4InitEPv");
    static const uint32_t expected[] = {0xe92d4070, 0xe2805004};
    if (!init || memcmp((void *)init, expected, sizeof(expected)))
        fatal_error("Gangstar: unsupported Vox audio initialization layout");
    gangstar_audio_hook = hook_addr(init, (uintptr_t)gangstar_audio_initialize);
    uintptr_t callback = so_symbol(&so_mod, "_ZN3vox13DriverAndroid11CallbackOSLEPKPK17SLBufferQueueItf_Pv");
    gangstar_vox_lock = (void *)so_symbol(&so_mod, "_ZN3vox5Mutex4LockEv");
    gangstar_vox_unlock = (void *)so_symbol(&so_mod, "_ZN3vox5Mutex6UnlockEv");
    gangstar_vox_fill = (void *)so_symbol(&so_mod, "_ZN3vox13DriverAndroid13DoCallbackOSLEv");
    static const uint32_t callback_expected[] = {0xe92d4070, 0xe2514000};
    if (!callback || !gangstar_vox_lock || !gangstar_vox_unlock || !gangstar_vox_fill ||
        memcmp((void *)callback, callback_expected, sizeof(callback_expected)))
        fatal_error("Gangstar: unsupported Vox audio callback layout");
    hook_addr(callback, (uintptr_t)gangstar_audio_callback);
}

#ifdef GANGSTAR_HIDE_TOUCH_BUTTONS
static void gangstar_skip_touch_draw(const void *unused) { (void)unused; }
static void gangstar_hide_touch_buttons(void) {
    // Draw-only hooks. gangstar_controls_init separately filters mapped touch
    // regions. Never suppress CHudManager's update or draw pass.
    static const struct { const char *symbol; uint32_t expected[2]; } draws[] = {
        {"_ZNK11AnalogStick6draw2dEv", {0xe92d4070, 0xe5903024}},
        {"_ZNK11SwipeButton6draw2dEv", {0xe52de004, 0xe5903024}},
        {"_ZNK12SlideControl6draw2dEv", {0xe92d4070, 0xe5903024}},
        {"_ZNK12SprintButton6draw2dEv", {0xe92d40f0, 0xe5903024}},
        {"_ZNK12ToggleButton6draw2dEv", {0xe52de004, 0xe5902024}},
        {"_ZNK13VirtualButton6draw2dEv", {0xe92d4010, 0xe5903024}},
        {"_ZNK14AnimatedButton6draw2dEv", {0xe92d4030, 0xed2d8b02}},
        {"_ZNK5Wheel6draw2dEv", {0xe92d4ff0, 0xed2d8b04}},
        {"_ZN11CHudManager15drawNitroButtonEPKv", {0xe59f30ac, 0xe92d4070}},
        {"_ZN11CHudManager17drawGrenadeButtonEPKv", {0xe92d47f0, 0xed2d8b02}},
        {"_ZN11CHudManager24drawNitroIndicatorButtonEPKv", {0xe59f30ac, 0xe92d4070}}
    };
    for (unsigned i = 0; i < sizeof(draws) / sizeof(*draws); ++i) {
        uintptr_t address = so_symbol(&so_mod, draws[i].symbol);
        if (!address || memcmp((const void *)address, draws[i].expected, 8)) {
            l_warn("Touch draw hide skipped: %s signature mismatch", draws[i].symbol);
            continue;
        }
        hook_addr(address, (uintptr_t)gangstar_skip_touch_draw);
    }
    l_info("Gangstar touch-button drawing hidden; HUD state retained");
}
#endif

void gangstar_native_startup_install(void);
void so_patch(void) {
	so_patch_module(&so_mod);
	uintptr_t console = so_symbol(&so_mod, "_ZN3glf7Console8PrintRawEPKc");
	if (console) hook_addr(console, (uintptr_t)gangstar_console_raw);
	gangstar_patch_hud_profile();
	gangstar_controls_init();
#ifdef GANGSTAR_HIDE_TOUCH_BUTTONS
	gangstar_hide_touch_buttons();
#endif
	gangstar_trace_audio_init();
	gangstar_native_startup_install();
}
