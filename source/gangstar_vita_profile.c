/* Applies source/gangstar_vita_profile.h to the game's selected phone profile.
 * Runs once on the initialization thread, from the customProfileForSpecialDevices
 * hook in patch.c, before the world, spawner or NPC manager read the profile. */
#include "gangstar_vita_profile.h"
#include "utils/logger.h"
#include <psp2/io/fcntl.h>

#define RIO_PROFILE_CONFIG DATA_PATH "perf.cfg"

void gangstar_vita_profile_apply(unsigned char *profile) {
    RioProfileSettings settings;
    rio_profile_defaults(&settings);
    // Optional user tuning; a missing file just means the built-in defaults.
    char text[2048];
    int length = 0;
    SceUID fd = sceIoOpen(RIO_PROFILE_CONFIG, SCE_O_RDONLY, 0);
    if (fd >= 0) {
        while (length < (int)sizeof(text)) {
            int n = sceIoRead(fd, text + length, sizeof(text) - (unsigned)length);
            if (n <= 0) break;
            length += n;
        }
        sceIoClose(fd);
        rio_profile_parse(&settings, text, (size_t)length);
        if (settings.errors)
            l_warn("Gangstar perf.cfg: %u line(s) ignored (unknown key or bad value)", settings.errors);
    }
    int before[RIO_PROFILE_FIELD_COUNT], after[RIO_PROFILE_FIELD_COUNT];
    int changed = rio_profile_apply(profile, &settings, before, after);
    if (changed < 0) {
        l_warn("Gangstar Vita performance profile skipped: unexpected profile layout");
        return;
    }
    l_info("RIO_VITA_PROFILE enabled=%d config=%s changed=%d", settings.enabled,
        fd >= 0 ? "perf.cfg" : "built-in", changed);
    for (unsigned i = 0; i < RIO_PROFILE_FIELD_COUNT; ++i)
        l_info("RIO_VITA_PROFILE %s=%d (stock %d)", rio_profile_fields[i].key, after[i], before[i]);
    (void)after; (void)before;
}

#ifndef GANGSTAR_RELEASE_BUILD
#include <so_util/so_util.h>
#include <string.h>
extern so_module so_mod;
/* Diagnostic readback of the values the engine is actually using, not the
 * profile fields this file wrote. Offsets from the 1.2.1g library:
 *  - UpdateFarPlaneDistance (called from CCustomSceneManager::drawAll every
 *    frame) converts profile +0x5c/+0x60 into two floats right after gPhonePerf
 *    (gPhonePerf+0x5c / +0x60); GetHiResFarPlaneDistance and drawAll read them.
 *  - NPCRoundRobinManager::UpdateProperties copies profile +0xbc into
 *    gNumberOfNPCPerFrame and (float)profile +0xb8 into the next word; the
 *    "test skippable animation" flag is the byte before it, and m_Instance is
 *    non-null once the manager exists. */
void gangstar_vita_profile_report(void) {
    static uintptr_t perf, npc_budget, npc_active, npc_instance;
    static int resolved;
    if (!resolved) {
        resolved = 1;
        perf = so_symbol(&so_mod, "gPhonePerf");
        npc_budget = so_symbol(&so_mod, "gNumberOfNPCPerFrame");
        npc_active = so_symbol(&so_mod, "gRoundRobinManagerActivated");
        npc_instance = so_symbol(&so_mod, "_ZN20NPCRoundRobinManager10m_InstanceE");
    }
    if (!perf || !npc_budget || !npc_active || !npc_instance) {
        l_info("RIO_VITA_PROFILE_LIVE unavailable=1");
        return;
    }
    unsigned char *profile; memcpy(&profile, (void *)perf, sizeof(profile));
    int32_t high = 0, low = 0, npc_per_frame_profile = 0, npc_distance_profile = 0;
    if (profile) {
        memcpy(&high, profile + 0x5c, 4); memcpy(&low, profile + 0x60, 4);
        memcpy(&npc_distance_profile, profile + 0xb8, 4); memcpy(&npc_per_frame_profile, profile + 0xbc, 4);
    }
    float live_high, live_low, live_distance;
    int32_t live_per_frame; uintptr_t instance;
    memcpy(&live_high, (void *)(perf + 0x5c), 4); memcpy(&live_low, (void *)(perf + 0x60), 4);
    memcpy(&live_per_frame, (void *)npc_budget, 4); memcpy(&live_distance, (void *)(npc_budget + 4), 4);
    memcpy(&instance, (void *)npc_instance, sizeof(instance));
    l_info("RIO_VITA_PROFILE_LIVE profile_high=%d live_high=%d profile_low=%d live_low=%d "
        "npc_manager=%d npc_active=%u skippable_test=%u profile_npc_per_frame=%d live_npc_per_frame=%d "
        "profile_npc_distance=%d live_npc_distance=%d",
        (int)high, (int)live_high, (int)low, (int)live_low, instance != 0,
        *(unsigned char *)npc_active, *(unsigned char *)(npc_budget - 1),
        (int)npc_per_frame_profile, (int)live_per_frame, (int)npc_distance_profile, (int)live_distance);
}
#endif
