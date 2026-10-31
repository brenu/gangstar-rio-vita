#include "gangstar_registration_profile.h"
#include "gangstar_audio_poll.h"
#include "gangstar_phase_timing.h"
#include "gangstar_render_sample.h"
#include "gangstar_startup_trampoline.h"
#ifdef GANGSTAR_SELECTIVE_VERTEX_COPY
#include "gangstar_vertex_scratch.h"
#endif
#include "gangstar_audio_probe.h"
#include "gangstar_audio_sched.h"
#include "gangstar_audio_chunk.h"
#include "gangstar_adpcm.h"
#include "gangstar_adpcm_stream.h"
#include "gangstar_native_perf.h"
#include "gangstar_wait_perf.h"
#include "gangstar_route_cache.h"
#include "gangstar_path_index.h"
#include "gangstar_pak_lookup.h"
#include "gangstar_archive_cache.h"
#include "gangstar_texture_lod.h"
#include "gangstar_district_preload.h"
#include "gangstar_asset_perf.h"
#include "gangstar_resident_policy.h"
#include "gangstar_texture_identity.h"
#include "gangstar_texture_pin.h"
#include "gangstar_stream_guard.h"
#include "utils/logger.h"
#include <so_util/so_util.h>
#include <kubridge.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdatomic.h>
#include <pthread.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
extern so_module so_mod;

#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
typedef uintptr_t (*NativeCall)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static struct Probe {
    const char *symbol, *label;
    uint32_t expected[2];
    NativeCall original;
    uint64_t us;
    unsigned calls;
} probes[] = {
    {"_ZN13ScriptManager11loadScriptsEv", "loadScripts", {0xe59f13e0, 0xe3a02000}, NULL, 0, 0},
    {"_ZN13ScriptManager14loadScriptFileEPKcN6Script5eTypeE", "loadScriptFile", {0xe59f3398, 0xe92d4ff0}, NULL, 0, 0},
    {"_ZN13ScriptManager15loadScriptNamesEv", "loadScriptNames", {0xe92d4ff0, 0xe1a07000}, NULL, 0, 0},
    {"_ZN13ScriptManager16loadDialogSoundsEv", "loadDialogSounds", {0xe92d45f0, 0xe59f818c}, NULL, 0, 0},
    {"_ZN13ScriptManager9loadPropsEv", "loadProps", {0xe59f15b4, 0xe92d4ff0}, NULL, 0, 0},
    {"_ZN13ScriptManager27loadChapterStartupScriptIDsEv", "loadChapterScriptIDs", {0xe92d47f0, 0xe59f60fc}, NULL, 0, 0},
    {"_ZN13ScriptManager23loadInterruptConditionsEv", "loadInterruptConditions", {0xe92d4ff0, 0xe59f918c}, NULL, 0, 0},
    {"_ZN3glf10FileStream4OpenEPKcNS_3ios8openflagE", "FileStream_Open", {0xe92d4ff0, 0xe59f6360}, NULL, 0, 0},
    {"_ZN12SoundManager4initEv", "SoundManager_init", {0xe92d4ff0, 0xe59030c4}, NULL, 0, 0},
    {"_ZN3vox8VoxUtils22LoadDataSourceFromFileEPKcii", "Vox_loadFile", {0xe92d40f0, 0xe24dd014}, NULL, 0, 0},
    {"_ZN3vox8VoxUtils27LoadDataSourceFromFileAsRAWEPKcii", "Vox_loadRaw", {0xe92d45f0, 0xe24dd02c}, NULL, 0, 0},
    {"_ZN3vox8VoxUtils27LoadDataSourceFromFileToRAMEPKcii", "Vox_loadRam", {0xe92d4ff0, 0xe24dd02c}, NULL, 0, 0},
    {"_ZN3vox9VoxEngine18ConvertToRawSourceERNS_10DataHandleE", "Vox_convertRaw", {0xe59f3048, 0xe92d4010}, NULL, 0, 0},
    {"_ZN17CCustomFileSystem17createAndOpenFileEPKc", "Rio_fileLookupOpen", {0xe92d4ff0, 0xe59f415c}, NULL, 0, 0},
    {"_ZN6glitch2io14CGlfFileSystem17createAndOpenFileEPKc", "Glf_fileLookupOpen", {0xe92d4ff0, 0xe59fa1f8}, NULL, 0, 0},
    {"_ZN17CCustomFileSystem17CreateAndOpenFileEPKc", "Rio_archiveDispatch", {0xe92d4ff0, 0xe1a06001}, NULL, 0, 0},
    {"_ZN16CCustomPakReader8openFileEPKc", "Rio_pakOpenName", {0xe92d4070, 0xe1a05001}, NULL, 0, 0},
    {"_ZN16CCustomPakReader8findFileEPKc", "Rio_pakFindName", {0xe92d4ff0, 0xe2514000}, NULL, 0, 0},
    {"_ZN21CStreamTextureManager6UpdateEv", "TextureStream_Update", {0xe92d4ff0, 0xe1a06000}, NULL, 0, 0},
    {"_ZN10FileStream4LoadEPKcPi", "FileStream_Load", {0xe92d40f0, 0xe24dd00c}, NULL, 0, 0},
    {"_ZN16CCustomPakReader8openFileEi", "Pak_OpenIndex", {0xe92d41f0, 0xe5913008}, NULL, 0, 0},
    {"_ZNK6glitch5video15CImageLoaderDDS15loadTextureDataEPNS_2io9IReadFileERKN5boost13intrusive_ptrINS0_8ITextureEEERKNS0_12STextureDescE", "DDS_loadData", {0xe92d47f0, 0xe24dd098}, NULL, 0, 0},
    {"_ZN16AnimationManager14loadAnimationsEPKc", "Animation_load", {0xe92d4ff0, 0xe59f4344}, NULL, 0, 0},
    {"_ZN16AnimationManager17loadAnimationSetsEPKc", "Animation_loadSets", {0xe92d4ff0, 0xe59f449c}, NULL, 0, 0},
    {"_ZN6glitch7collada8CResFileC1EPKcRKN5boost13intrusive_ptrINS_2io9IReadFileEEEb", "Resource_ctor", {0xe92d41f0, 0xe59f51c0}, NULL, 0, 0},
    {"_ZN9GS3DStuff4loadEiRi", "World_load", {0xe92d4ff0, 0xe59f4dcc}, NULL, 0, 0},
    {"_ZN6glitch4task23CCpuGraphicsTaskHandler3runEb", "GraphicsTask_run", {0xe92d47f0, 0xe1a07000}, NULL, 0, 0},
    {"_ZN6glitch5video15CTextureManager32unloadTexturesMarkedAsUnloadableEv", "AutoTextureUnload", {0xe92d4070, 0xe1a05000}, NULL, 0, 0},
    {"_ZN6glitch5video8ITexture6unbindEjj", "Texture_unbind", {0xe92d4070, 0xe5d03038}, NULL, 0, 0},
    {"_ZN6glitch5video15CTextureManager13unloadTextureERKN5boost13intrusive_ptrINS0_8ITextureEEE", "Texture_unload", {0xe92d4010, 0xe5913000}, NULL, 0, 0},
    {"_ZN6glitch5video15CTextureManager13removeTextureEPNS0_8ITextureE", "Texture_remove", {0xe92d40f0, 0xe2513000}, NULL, 0, 0},
    {"_ZN6glitch5video8ITextureD2Ev", "Texture_destroy", {0xe59fc0a8, 0xe3a01000}, NULL, 0, 0},
    {"_ZN6glitch5video15CCommonGLDriverINS0_21CProgrammableGLDriverINS0_30CProgrammableShaderHandlerBaseINS0_18CGLSLShaderHandlerEEEEENS0_6detail33CProgrammableGLFunctionPointerSetEE8CTexture10unbindImplEjj", "GPU_release", {0xe92d47f0, 0xe590a02c}, NULL, 0, 0},
    {"_ZN6glitch5video15CTextureManager9removeAllEv", "Texture_clearAll", {0xe92d4ff0, 0xe590507c}, NULL, 0, 0},
    {"_ZN6glitch5video15CTextureManagerD2Ev", "Texture_managerDestroy", {0xe92d41f0, 0xe1a06000}, NULL, 0, 0},
    {"_ZN9GS3DStuff13streamingInitEv", "Streaming_init", {0xe59f3040, 0xe3a02003}, NULL, 0, 0},
    {"_ZN6glitch5video15CTextureManager10getTextureEPKcS3_", "Texture_getName", {0xe92d47f0, 0xe3530000}, NULL, 0, 0},
    {"_ZN6glitch5video15CTextureManager10getTextureERKN5boost13intrusive_ptrINS_2io9IReadFileEEEPKcb", "Texture_getFile", {0xe92d4ff0, 0xe1a06002}, NULL, 0, 0},
    {"_ZN6glitch5video15CTextureManager18getTextureInternalEPNS_2io9IReadFileERKSbIcSt11char_traitsIcENS_4core10SAllocatorIcLNS_6memory13E_MEMORY_HINTE0EEEEbRKN5boost13intrusive_ptrINS0_8ITextureEEESK_", "Texture_getInternal", {0xe92d4ff0, 0xe59f5978}, NULL, 0, 0},
    {"_ZN3vox10EmitterObj6UpdateEf", "Emitter_update", {0xe92d41f0,0xed2d8b02}, NULL,0,0},
    {"_ZN3vox10EmitterObj9LoadAsyncEv", "Emitter_loadAsync", {0xe92d45f0,0xe5903118}, NULL,0,0},
    {"_ZN3vox10EmitterObj9UpdateDSPEf", "Emitter_DSP", {0xe92d4070,0xe59030f0}, NULL,0,0},
    {"_ZN3vox10EmitterObj20ProcessNonNativeDataEf", "Emitter_process", {0xe92d4010,0xe5903110}, NULL,0,0},
    {"_ZN3vox17DecoderMPC8Cursor6DecodeEPvi", "Decode_MPC", {0xe92d47f0,0xe1a04000}, NULL,0,0},
    {"_ZN3vox18DecoderMSWavCursor6DecodeEPvi", "Decode_WAV", {0xe92d4010,0xe5903024}, NULL,0,0},
    {"_ZN3vox19DecoderNativeCursor6DecodeEPvi", "Decode_native", {0xe92d4070,0xe1a04000}, NULL,0,0},
    {"_ZN3vox22DecoderStbVorbisCursor6DecodeEPvi", "Decode_vorbis", {0xe92d4070,0xe1a04000}, NULL,0,0}
#ifdef GANGSTAR_NATIVE_ADPCM_VERIFY
    ,{"_ZN3vox26VoxMSWavSubDecoderIMAADPCM11DecodeBlockEPv", "Block_IMA", {0xe92d4ff0,0xe24dd06c}, NULL,0,0}
    ,{"_ZN3vox25VoxMSWavSubDecoderMSADPCM11DecodeBlockEPv", "Block_MS", {0xe92d4ff0,0xe5902008}, NULL,0,0}
    ,{"_ZN3vox26VoxMSWavSubDecoderIMAADPCM4SeekEj", "IMA_seek", {0xe92d41f0,0xe1a05001}, NULL,0,0}
#endif
    ,{"_ZN11Application6UpdateEv","Phase_application",{0xe92d41f0,0xe1a05000},NULL,0,0}
    ,{"_ZN9GS3DStuff6updateEP12StateMachine","Phase_game_update",{0xe92d4ff0,0xed2d8b06},NULL,0,0}
    ,{"_ZN9GS3DStuff4drawEv","Phase_draw",{0xe59f042c,0xe59f142c},NULL,0,0}
    ,{"_ZN7b2World4StepEfii","Phase_physics",{0xe92d41f0,0xed2d8b02},NULL,0,0}
    ,{"_ZN5World15UpdateStreamingERN6glitch4core8vector3dIfEEff","Phase_world_stream",{0xe92d4ff0,0xed2d8b04},NULL,0,0}

    ,{"_ZN11Application5_DrawEv","Phase_render_frame",{0xe92d4ff0,0xe59f4e84},NULL,0,0}
    ,{"_ZN4glot15TrackingManager6UpdateEi","Phase_tracking",{0xe92d41f0,0xe3510000},NULL,0,0}
    ,{"_ZN6glitch7IDevice3runEv","Phase_device",{0xe92d4010,0xe1a04000},NULL,0,0}
    ,{"_ZN3glf12EventManager14DispatchEventsEv","Phase_events",{0xe92d41f0,0xe59f70cc},NULL,0,0}

    ,{"_ZN3glf12InputManager11UpdateFrameEv","Phase_input",{0xe92d4ff0,0xe5909010},NULL,0,0}
    ,{"_ZN11Application8PostInitEv","Phase_post_init",{0xe92d47f0,0xe24dd008},NULL,0,0}

    ,{"_ZN11Application7_UpdateEi","Phase_update_frame",{0xe92d4ff0,0xed2d8b02},NULL,0,0}
    ,{"_ZN11Application18updateDelayedTasksEi","Phase_delayed_tasks",{0xe3073b54,0xe3403001},NULL,0,0}
    ,{"_ZN7MenuMgr6UpdateEi","Phase_menu_update",{0xe92d4ff0,0xed2d8b02},NULL,0,0}
    // Soft-float ABI passes StateMachine::update(double) in r0/r2/r3;
    // the four-word phase wrapper forwards those argument bits unchanged.
    ,{"_ZN12StateMachine6updateEd","Phase_state_update",{0xe92d4070,0xe1c022f0},NULL,0,0}
    ,{"_ZN17GameObjectManager5PrintEv","Phase_object_stats",{0xe92d4070,0xe59f406c},NULL,0,0}
    ,{"_ZN6GSMain6updateEP12StateMachine","Phase_main_state",{0xe92d4ff0,0xed2d8b02},NULL,0,0}
    ,{"_ZN6GSMain11LoadingMenu6updateEPS_","Phase_loading_state",{0xe92d45f0,0xe24dd014},NULL,0,0}
    ,{"_ZN9GS3DStuff4loadEiRi","Phase_world_load",{0xe92d4ff0,0xe59f4dcc},NULL,0,0}
    ,{"_ZN12StateMachine12_switchStateEP9StateBase","Phase_state_switch",{0xe92d4070,0xe5903014},NULL,0,0}
    ,{"_ZN12StateMachine10_pushStateEP9StateBase","Phase_state_push",{0xe92d47f0,0xe1a04000},NULL,0,0}
    ,{"_ZN12StateMachine8popStateEv","Phase_state_pop",{0xe92d4010,0xe5903014},NULL,0,0}
    ,{"_ZN19CCustomSceneManager7drawAllERKN5boost13intrusive_ptrIN6glitch5scene10ISceneNodeEEEbb","Phase_scene_custom",{0xe92d4ff0,0xed2d8b06},NULL,0,0}
    ,{"_ZN6glitch5scene13CSceneManager7drawAllERKN5boost13intrusive_ptrINS0_10ISceneNodeEEEbb","Phase_scene_draw",{0xe92d41f0,0xe1a05002},NULL,0,0}
    ,{"_ZN6glitch5scene13CSceneManager11renderListsEPNS_5video12IVideoDriverEb","Phase_render_lists",{0xe92d4ff0,0xed2d8b02},NULL,0,0}
    ,{"_ZN6glitch5scene13CSceneManager18renderListsCompileEPNS_5video12IVideoDriverE","Phase_compile_lists",{0xe92d4ff0,0xed2d8b02},NULL,0,0}
    ,{"_ZN6glitch5scene13CSceneManager19drawShadowReceiversEv","Phase_shadow_receivers",{0xe92d4ff0,0xe5902090},NULL,0,0}
    ,{"_ZN11PostEffects7PreDrawEv","Phase_effects_pre",{0xe92d47f0,0xe59f4180},NULL,0,0}
    ,{"_ZN11PostEffects8PostDrawEv","Phase_effects_post",{0xe92d45f0,0xe24dd014},NULL,0,0}
    ,{"_ZNK12StateMachine4drawEv","Phase_state_draw",{0xe92d4010,0xe5903014},NULL,0,0}
    ,{"_ZN5World16UpdateBillboardsEv","Phase_billboards",{0xe5900020,0xe3500000},NULL,0,0}
    ,{"_ZN7DayTime14RegisterLightsEPN6glitch5scene13CSceneManagerE","Phase_day_lights",{0xe92d4ff0,0xe24dd02c},NULL,0,0}
    ,{"_ZN6glitch4core8heapsortINS_5scene13CSceneManager17SDefaultNodeEntryEEEvPT_i","Phase_sort_opaque",{0xe92d4ff0,0xe24dd0b4},NULL,0,0}
    ,{"_ZN6glitch4core8heapsortINS_5scene13CSceneManager21STransparentNodeEntryEEEvPT_i","Phase_sort_transparent",{0xe92d4ff0,0xe24dd0b4},NULL,0,0}
    ,{"_ZN23CLowResCitySceneManager13drawAllLowResEPN6glitch5scene13CSceneManagerE","Phase_lowres_city",{0xe92d47f0,0xed2d8b02},NULL,0,0}
    ,{"_ZN6glitch5scene13CSceneManager18registerSceneNodesERKN5boost13intrusive_ptrINS0_10ISceneNodeEEE","Phase_register_tree",{0xe92d4070,0xe59030d8},NULL,0,0}
    ,{"_ZN6glitch5scene13CSceneManager18registerSceneNodesERKSt6vectorIN5boost13intrusive_ptrINS0_10ISceneNodeEEENS_4core10SAllocatorIS6_LNS_6memory13E_MEMORY_HINTE0EEEE","Phase_register_vector",{0xe92d4070,0xe59030d8},NULL,0,0}
    ,{"_ZN11PostEffects24UpdateHDWaterFrameBufferEv","Phase_water_framebuffer",{0xe92d40f0,0xe24dd014},NULL,0,0}
    ,{"_ZN13CCustomCuller5startEPN6glitch5scene13CSceneManagerERKN5boost13intrusive_ptrINS1_10ISceneNodeEEE","Phase_culler_start",{0xe92d41f0,0xe3a03000},NULL,0,0}
    ,{"_ZN13CCustomCuller9cullStartEPN6glitch5scene13CSceneManagerE","Phase_culler_prepare",{0xe92d4ff0,0xe59110d8},NULL,0,0}
    ,{"_ZN6glitch5scene15getFrustumBoxesERKNS0_12SViewFrustumERKNS0_5SGridERSt6vectorINS_4core8vector2dIiEESaISA_EERS7_INS0_11SBlockRangeESaISE_EE","Phase_frustum_boxes",{0xe92d4ff0,0xed2d8b04},NULL,0,0}
    ,{"_ZN6glitch5scene18CStreamedBatchMeshI14SSegmentExtPtr23ExtraDataHandlingPolicyNS0_25SProactiveMemoryAllocatorEE12flushSubDataEv","Phase_stream_mesh_flush",{0xe92d4ff0,0xe2801f83},NULL,0,0}
    ,{"_ZNK6glitch5scene21CRegularGridStreamingItE23getIteratorsForGridCellERKNS_4core8vector3dIiEERNS_4util10CRLEBitSetIjNS3_10SAllocatorIjLNS_6memory13E_MEMORY_HINTE0EEEE18CRLEBitSetIteratorESG_","Phase_grid_lookup_sampled",{0xe92d41f0,0xe1a06002},NULL,0,0}
    ,{"_ZNK6glitch5scene18CStreamedBatchMeshI14SSegmentExtPtr23ExtraDataHandlingPolicyNS0_25SProactiveMemoryAllocatorEE10getSegmentEj","Phase_segment_lookup_sampled",{0xe92d4010,0xe24dd010},NULL,0,0}
    ,{"_ZN3glf17ReadWriteSpinLock12readLockImplEj","Phase_read_lock_sampled",{0xe92d47f0,0xe1a04000},NULL,0,0}
    ,{"_ZN3glf17ReadWriteSpinLock10readUnlockEv","Phase_read_unlock_sampled",{0xe92d4010,0xe1a04000},NULL,0,0}
    ,{"_ZN3glf6Thread21GetSequentialThreadIdEv","Cached_sequential_thread_id",{0xe92d4070,0xe59f4080},NULL,0,0}
    ,{"_ZN6glitch5video12IVideoDriver11setMaterialERKN5boost13intrusive_ptrINS0_9CMaterialEEEhPKNS3_INS0_19CVertexAttributeMapEEE","Sample_material_submit",{0xe92d4ff0,0xe1a07003},NULL,0,0}
    ,{"_ZN6glitch5scene14CMeshSceneNode6renderEPv","Sample_mesh_render",{0xe92d4ff0,0xe5902108},NULL,0,0}
    ,{"_ZN6glitch5video12IVideoDriver19setMaterialInternalEPNS0_9CMaterialEhPKN5boost13intrusive_ptrINS0_19CVertexAttributeMapEEE","Sample_material_internal",{0xe92d4030,0xe590c0dc},NULL,0,0}
    ,{"_ZNK6glitch5video9CMaterial9cloneHeapEPKc","Sample_material_clone",{0xe92d41f0,0xe2516000},NULL,0,0}
    ,{"_ZN6glitch4core18allocProcessBufferEi","Sample_process_alloc",{0xe92d4010,0xe1a04000},NULL,0,0}
    ,{"_ZN6glitch5video6detail19IMaterialParametersINS0_9CMaterialENS_24ISharedMemoryBlockHeaderIS3_EEE14dropParametersEv","Sample_material_drop",{0xe92d4070,0xe5903004},NULL,0,0}
    ,{"_ZN6glitch5video9CMaterial4initEPKhPKNS0_6detail8material12SRenderStateEb","Sample_material_init",{0xe92d4070,0xe1a06003},NULL,0,0}
    ,{"_ZN6glitch4core6detail22SSharedStringHeapEntry5SData3getEPKcb","Sample_shared_string",{0xe92d4ff0,0xed2d8b02},NULL,0,0}

};
// Generic probes use r0-r3 and return at most one word. Probe 37 has an
// explicit five-argument wrapper; probe 38 has seven arguments.
// A permanent trampoline keeps the original callable even on other threads;
// never temporarily restore the function's entry instructions around a call.
#define PROBE(N) \
static __attribute__((noinline)) uintptr_t probe_##N(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) { \
    uint64_t audio_began = (N>=9 && N<=12 && rio_audio_scope_active()) ? sceKernelGetProcessTimeWide() : 0; \
    uint64_t began = gangstar_wait_begin(); \
    uintptr_t result = probes[N].original(a, b, c, d); \
    uint64_t elapsed = gangstar_wait_end(RIO_WAIT_NATIVE, began, (uintptr_t)__builtin_return_address(0), \
        a, 0, (int)result, probes[N].label); \
    if (audio_began) rio_audio_phase(RIO_AUDIO_FILE+N-9,sceKernelGetProcessTimeWide()-audio_began); \
    if (began) { probes[N].us += elapsed; ++probes[N].calls; } \
    return result; \
}
PROBE(0) PROBE(1) PROBE(2) PROBE(3) PROBE(4) PROBE(5) PROBE(6) PROBE(7)
PROBE(8) PROBE(9) PROBE(10) PROBE(11) PROBE(12)
PROBE(13) PROBE(14) PROBE(18) PROBE(22) PROBE(23) PROBE(24) PROBE(25) PROBE(26)
// Main-thread only, fixed storage; report after a severe frame instead of
// writing logs while an asset is being loaded. Retain the 16 longest loads.
static struct AssetLoad { uint64_t us; unsigned bytes; const char *op; char name[192]; } asset_loads[16];
static unsigned asset_count, asset_dropped;
static void record_asset_load(uint64_t elapsed, unsigned bytes, const char *name, const char *op) {
    if (elapsed < 1000) return;
    unsigned slot = asset_count;
    if (slot == 16) {
        ++asset_dropped; slot = 0;
        for (unsigned i = 1; i < 16; ++i)
            if (asset_loads[i].us < asset_loads[slot].us) slot = i;
        if (elapsed <= asset_loads[slot].us) return;
    } else ++asset_count;
    struct AssetLoad *load = &asset_loads[slot];
    load->us = elapsed; load->bytes = bytes; load->op = op;
    unsigned i = 0;
    if (name) for (; i < sizeof(load->name) - 1 && name[i]; ++i) {
        unsigned char ch = name[i]; load->name[i] = ch >= 32 && ch < 127 ? ch : '?';
    }
    load->name[i] = 0;
}
static uintptr_t probe_19(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    uint64_t began = gangstar_wait_begin();
    uintptr_t result = probes[19].original(a, b, c, d);
    uint64_t elapsed = gangstar_wait_end(RIO_WAIT_NATIVE, began,
        (uintptr_t)__builtin_return_address(0), a, 0, (int)result, probes[19].label);
    if (began) {
        probes[19].us += elapsed; ++probes[19].calls;
        record_asset_load(elapsed, result && b && *(int *)b > 0 ? *(int *)b : 0,
            (const char *)a, probes[19].label);
    }
    return result;
}
#ifdef GANGSTAR_ARCHIVE_ROUTE_CACHE
static RioRouteCache route_cache;
static uintptr_t pak_vtable;
static unsigned route_hits, route_misses, route_learned, route_failed, route_packed_misses, route_skipped;
static atomic_uint route_max_readers, route_max_folders;
#ifdef GANGSTAR_DIRECT_ASSET_PATHS
typedef struct { uintptr_t reader, file; unsigned generation; RioPathIndex index; } RioNativePathIndex;
static RioNativePathIndex path_indexes[4];
#ifdef GANGSTAR_AUDIO_DIRECT_PATHS
/* Only accessed under the verified outer native filesystem write lock.
   Main-thread reporting never touches this background index state. */
static RioNativePathIndex audio_path_indexes[4];
static atomic_uint audio_path_opened,audio_path_missing,audio_path_fallback;
#endif
static unsigned path_hits, path_missing, path_fallback;
static uint64_t path_us, path_worst;
static uintptr_t probe_20(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
#endif
static struct Capture {
    uintptr_t reader, file;
    char resolved[RIO_ROUTE_PATH];
} *capture;
// OBB metadata is immutable. Only filesystem configuration mutations advance
// this generation; all five mutation/lifetime hooks must install to enable it.
static atomic_uint route_generation=1;
static int route_generation_ready;
static atomic_uint route_stamp_fast,route_stamp_full;
static unsigned route_entry_hits;
static void mix(uint64_t *hash, uintptr_t value) { *hash = (*hash ^ value) * UINT64_C(1099511628211); }
// Called only from the verified outer createAndOpenFile call sites, which
// already hold Rio's global recursive filesystem write lock.
static int route_stamp(uintptr_t parent, uint64_t *hash, int *all_packed) {
    uintptr_t *p = (uintptr_t *)parent;
    uintptr_t first = p[0x30 / 4], last = p[0x34 / 4];
    uintptr_t folders = p[0x3c / 4], folder_end = p[0x40 / 4];
    if (last >= first && (last - first) / 4 > route_max_readers) route_max_readers = (last - first) / 4;
    if (folder_end >= folders && (folder_end - folders) / 4 > route_max_folders) route_max_folders = (folder_end - folders) / 4;
    if (last < first || folder_end < folders || ((first | last | folders | folder_end) & 3) ||
        (last - first) > 65536 || (folder_end - folders) > 65536 ||
        (!first && last != first) || (!folders && folder_end != folders)) return 0;
    *all_packed = 1;
    *hash = UINT64_C(1469598103934665603);
    mix(hash, first); mix(hash, last); mix(hash, folders); mix(hash, folder_end);
    for (uintptr_t at = first; at < last; at += 4) {
        uintptr_t reader = *(uintptr_t *)at;
        mix(hash, reader);
        if (reader && *(uintptr_t *)reader == pak_vtable) {
            mix(hash, *(uintptr_t *)(reader + 8));
            mix(hash, *(uintptr_t *)(reader + 0x20));
            mix(hash, *(uintptr_t *)(reader + 0x24));
            mix(hash, *(unsigned char *)(reader + 0x2c));
            mix(hash, *(unsigned char *)(reader + 0x2d));
        } else *all_packed = 0;
    }
    if(route_generation_ready && *all_packed) {
        mix(hash,atomic_load(&route_generation));
        atomic_fetch_add(&route_stamp_fast,1);return 1;
    }
    atomic_fetch_add(&route_stamp_full,1);
    for (uintptr_t at = folders; at < folder_end; at += 4) {
        const unsigned char *name = *(const unsigned char **)at;
        mix(hash, (uintptr_t)name);
        if (!rio_route_mix_path(hash,(const char *)name)) return 0;
    }
    return 1;
}
static int reader_present(uintptr_t parent, uintptr_t reader) {
    uintptr_t *p = (uintptr_t *)parent;
    for (uintptr_t at = p[0x30 / 4]; at < p[0x34 / 4]; at += 4)
        if (*(uintptr_t *)at == reader) return *(uintptr_t *)reader == pak_vtable;
    return 0;
}
#ifdef GANGSTAR_DIRECT_ASSET_PATHS
static int direct_path_open(uintptr_t out, uintptr_t parent, const char *query, uintptr_t extra, uint64_t stamp,
    RioNativePathIndex *indexes, RioRouteCache *routes) {
    int main_index=indexes==path_indexes;
    uintptr_t first=*(uintptr_t *)(parent+0x30), last=*(uintptr_t *)(parent+0x34);
    uintptr_t folders=*(uintptr_t *)(parent+0x3c), folder_end=*(uintptr_t *)(parent+0x40);
    if ((last-first)/4>4) { if(main_index)++path_fallback; return 0; }
    for (uintptr_t at=first; at<last; at+=4) {
        uintptr_t reader=*(uintptr_t *)at;
        if (!reader || *(uintptr_t *)reader!=pak_vtable) { if(main_index)++path_fallback; return 0; }
        uintptr_t entries=*(uintptr_t *)(reader+0x20), end=*(uintptr_t *)(reader+0x24);
        if (end<entries || ((end-entries)&15) || (entries&3) || (!entries && end!=entries)) {
            if(main_index)++path_fallback; return 0;
        }
        unsigned slot=(unsigned)((at-first)/4);
        uintptr_t file=*(uintptr_t *)(reader+8);
        unsigned generation=route_generation_ready?atomic_load(&route_generation):0;
        if (indexes[slot].reader!=reader || indexes[slot].file!=file || indexes[slot].generation!=generation) {
            rio_path_index_clear(&indexes[slot].index);
            indexes[slot].reader=reader; indexes[slot].file=file; indexes[slot].generation=generation;
        }
        RioPathIndex *index=&indexes[slot].index;
        if (!rio_path_index_prepare(index,(const RioPakEntry *)entries,(end-entries)/16,
            *(unsigned char *)(reader+0x2c),(const char *const *)folders,(folder_end-folders)/4,stamp)) {
            if(main_index)++path_fallback; return 0;
        }
        int found=rio_path_index_find(index,query,*(unsigned char *)(reader+0x2d));
        if (found==INT_MIN) { if(main_index)++path_fallback; return 0; }
        if (found>=0) {
            // Keep native file construction, ownership, source mip reduction
            // and archive seek locking. Only filename resolution is replaced.
            probe_20(out,reader,(uintptr_t)found,extra);
            if (!*(uintptr_t *)out) { if(main_index)++path_fallback; return 0; }
            if(routes) {
                rio_route_store(routes,parent,stamp,query,reader,index->entries[found].name);
                RioRoute *route=rio_route_find(routes,parent,stamp,query);
                if(route)route->entry=found;
            }
            if(main_index)++path_hits; return 1;
        }
    }
    // Exhaustive metadata search found no packed entry. Loose storage remains
    // live and is opened anew; archive misses can also feed the existing LRU.
    if(routes)rio_route_store_packed_miss(routes,parent,stamp,query);
    probes[14].original(out,parent,(uintptr_t)query,extra);
    if(main_index)++path_missing; return 1;
}
#endif
static uintptr_t route_dispatch(uintptr_t out, uintptr_t parent, uintptr_t name, uintptr_t extra,
    uintptr_t caller, uint64_t began) {
    uint64_t stamp; int all_packed;
#ifdef GANGSTAR_AUDIO_DIRECT_PATHS
    if (!began && rio_audio_scope_active() && pak_vtable && probes[14].original && probes[20].original &&
        (caller==so_mod.load_addr+0x467b7c || caller==so_mod.load_addr+0x467bdc) &&
        rio_route_name((const char *)name) && route_stamp(parent,&stamp,&all_packed) && all_packed) {
        int handled=direct_path_open(out,parent,(const char *)name,extra,stamp,audio_path_indexes,NULL);
        if(handled) {
            atomic_fetch_add(*(uintptr_t *)out?&audio_path_opened:&audio_path_missing,1);
            return out;
        }
        atomic_fetch_add(&audio_path_fallback,1);
    }
#endif
    int eligible = began && pak_vtable && probes[14].original && probes[16].original &&
        (caller == so_mod.load_addr + 0x467b7c || caller == so_mod.load_addr + 0x467bdc) &&
        rio_route_name((const char *)name) && route_stamp(parent, &stamp, &all_packed);
    if (!eligible) { if (began) ++route_skipped; return probes[15].original(out, parent, name, extra); }
    RioRoute *r = rio_route_find(&route_cache, parent, stamp, (const char *)name);
    if (r && r->packed_miss && all_packed) {
        ++route_packed_misses;
        // The verified outer caller already holds the filesystem write lock.
        // Retry loose storage every time; never cache loose-file absence.
        return probes[14].original(out, parent, name, extra);
    }
    if (r && r->reader && reader_present(parent, r->reader)) {
        // A cached entry opens by number: no filename search or folder walk.
        uintptr_t entries=*(uintptr_t *)(r->reader+0x20),end=*(uintptr_t *)(r->reader+0x24);
        if(r->entry>=0 && probes[20].original && end>=entries &&
            (unsigned)r->entry<(end-entries)/16) {
            probe_20(out,r->reader,(uintptr_t)r->entry,extra);
            if(*(uintptr_t *)out) {++route_hits;++route_entry_hits;return out;}
        }
        char resolved[RIO_ROUTE_PATH]; strcpy(resolved, r->resolved);
        uintptr_t result = probes[16].original(out, r->reader, (uintptr_t)resolved, extra);
        if (*(uintptr_t *)out) { ++route_hits; return result; }
        r = rio_route_find(&route_cache, parent, stamp, (const char *)name);
        if (r) r->reader = 0;
        ++route_failed;
    }
    ++route_misses;
#ifdef GANGSTAR_DIRECT_ASSET_PATHS
    if (all_packed && probes[20].original) {
        uint64_t start=sceKernelGetProcessTimeWide();
        int handled=direct_path_open(out,parent,(const char *)name,extra,stamp,path_indexes,&route_cache);
        uint64_t elapsed=sceKernelGetProcessTimeWide()-start;
        path_us+=elapsed; if (elapsed>path_worst) path_worst=elapsed;
        if (handled) return out;
    }
#endif
    struct Capture current = {0}, *previous = capture;
    capture = &current;
    uintptr_t result = probes[15].original(out, parent, name, extra);
    capture = previous;
    uint64_t after; int after_packed;
    if (*(uintptr_t *)out && *(uintptr_t *)out == current.file && current.reader && route_stamp(parent, &after, &after_packed) && after == stamp &&
        reader_present(parent, current.reader)) {
        rio_route_store(&route_cache, parent, stamp, (const char *)name, current.reader, current.resolved);
        ++route_learned;
    } else if (all_packed && !current.file && route_stamp(parent, &after, &after_packed) &&
        after_packed && after == stamp) {
        rio_route_store_packed_miss(&route_cache, parent, stamp, (const char *)name);
    }
    return result;
}
#endif
static uintptr_t probe_15(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    uint64_t began = gangstar_wait_begin();
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
#ifdef GANGSTAR_ARCHIVE_ROUTE_CACHE
    uintptr_t result = route_dispatch(a, b, c, d, caller, began);
#else
    uintptr_t result = probes[15].original(a, b, c, d);
#endif
    uint64_t elapsed = gangstar_wait_end(RIO_WAIT_NATIVE, began, caller, b, 0, (int)result, probes[15].label);
    if (began) { probes[15].us += elapsed; ++probes[15].calls; }
    return result;
}
static uintptr_t probe_16(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    uintptr_t result = probes[16].original(a, b, c, d);
#ifdef GANGSTAR_ARCHIVE_ROUTE_CACHE
    // Hundreds of thousands of unsuccessful searches: no clocks/thread checks
    // on misses. Only a successful result can teach a main-thread route.
    if (*(uintptr_t *)a && gangstar_wait_is_main() && capture && *(uintptr_t *)b == pak_vtable && rio_route_name((const char *)c)) {
        capture->reader = b; capture->file = *(uintptr_t *)a;
        strcpy(capture->resolved, (const char *)c);
    }
#endif
    return result;
}
#ifdef GANGSTAR_PAK_NAME_LOOKUP
static atomic_uint pak_fast, pak_fallback;
#endif
static uintptr_t probe_17(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
#ifdef GANGSTAR_PAK_NAME_LOOKUP
    if (!b) { atomic_fetch_add(&pak_fast, 1); return (uintptr_t)-1; }
    uintptr_t first = *(uintptr_t *)(a + 0x20), last = *(uintptr_t *)(a + 0x24);
    if (last >= first && !((last - first) & 15) && !(first & 3) && (first || first == last)) {
        int index = rio_pak_find((const RioPakEntry *)first, (last - first) / 16,
            (const char *)b, *(unsigned char *)(a + 0x2c), *(unsigned char *)(a + 0x2d));
        if (index != INT_MIN) { atomic_fetch_add(&pak_fast, 1); return (uintptr_t)(intptr_t)index; }
    }
    atomic_fetch_add(&pak_fallback, 1);
#endif
    return probes[17].original(a, b, c, d);
}
// Enqueue only immutable texture entries backed directly by an original OBB.
// Never retain native objects, seek them from the worker, or invoke game/GL code there.
static uintptr_t direct_file_vtable;
static uintptr_t (*make_memory_file)(uintptr_t, const void *, uint32_t, const char *, int);
static uintptr_t probe_20(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
#if defined(GANGSTAR_ASSET_PREFETCH) || defined(GANGSTAR_SMALL_LIGHTMAPS)
    if (direct_file_vtable && gangstar_wait_is_main()) {
        uintptr_t first = *(uintptr_t *)(b + 0x20), last = *(uintptr_t *)(b + 0x24);
        uintptr_t file = *(uintptr_t *)(b + 8);
        if (file && *(uintptr_t *)file == direct_file_vtable && first && last >= first &&
            !((last - first) & 15) && c < (last - first) / 16) {
            const RioPakEntry *entry = (const RioPakEntry *)(first + c * 16);
            const char *name = entry->name;
            size_t n = name ? strnlen(name, 256) : 0;
            if (!(entry->extra & 0x10000) && n >= 4 && n < 256 &&
                (!strcmp(name + n - 4, ".tga") || !strcmp(name + n - 4, ".pvr") || !strcmp(name + n - 4, ".dds"))) {
                uintptr_t *vtable = *(uintptr_t **)file;
                const char *path = ((const char *(*)(uintptr_t))vtable[0x28 / 4])(file);
                if (path) {
                    const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
                    int archive = !strcmp(base, "main.12030.com.gameloft.android.ANMP.GloftG4HM.obb") ? 0 :
                        !strcmp(base, "patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb") ? 1 : -1;
                    if (archive >= 0) {
#ifdef GANGSTAR_SMALL_LIGHTMAPS
                        if (make_memory_file && archive == 0 && rio_lod_cache_source(name)) {
                            uint32_t length = 0;
                            const void *small = rio_lod_original_source(name) ?
                                gangstar_texture_original_get(archive,entry->offset,entry->size,name,&length) :
                                gangstar_texture_lod_get(archive,entry->offset,entry->size,name,&length);
                            if (small) {
                                // Hidden C++ return pointer in a; transfer ownership
                                // so the native file frees this buffer when released.
                                uintptr_t result = make_memory_file(a, small, length, name, 1);
                                if (*(uintptr_t *)a) return result;
                                free((void *)small);
                            }
                        }
#endif
                        gangstar_archive_prefetch_asset(archive, entry->offset, entry->size);
                    }
                }
            }
        }
    }
#endif
    return probes[20].original(a, b, c, d);
}
// Bounded, main-thread-only history. Exact names avoid hash collisions;
// object addresses are observations, not proof of native object lifetimes.
static unsigned reuse_frame, reuse_count, reuse_dropped;
static struct { char name[192]; unsigned size, seen; } reuse_history[512];
static struct { char name[192]; unsigned frame, occurrence, size, calls;
    uintptr_t texture; uint64_t us, upload_us, read_us, read_bytes;
} reuse_events[128];
static unsigned reuse_history_count;
static RioTextureIdentities texture_identities;
static struct {
    const char *op; char name[192]; uintptr_t object, caller;
    unsigned frame, last_load_frame, load_serial, mask, ready, bound, removed;
    uint64_t us;
} eviction_events[64];
static unsigned eviction_count, eviction_dropped, eviction_filtered, eviction_unknown[4], eviction_calls[4];
#ifdef GANGSTAR_TEXTURE_REF_CACHE
#ifdef GANGSTAR_TEXTURE_PREWARM
static atomic_int texture_prewarm_done;
static uintptr_t (*texture_prewarm_manager)(void);
static NativeCall texture_prewarm_get;
static uintptr_t texture_prewarm_owner;
static unsigned texture_pin_hits;
#ifdef GANGSTAR_WORLD_SOURCE_PREWARM
static uintptr_t world_source_fs_vtable;
static void (*world_source_file_release)(uintptr_t);
static int world_source_preload_done;
#endif
static struct { unsigned frame, mode; uintptr_t manager, caller, object; unsigned flags;
    const char *op; char name[128], hash[128]; } texture_requests[48];
static unsigned texture_request_count, texture_request_dropped;
static void pin_request(const char *op, uintptr_t manager, const char *name,
    const char *hash, unsigned mode, uintptr_t caller);
#endif
static RioTexturePins texture_pins;
static pthread_mutex_t texture_pin_lock = PTHREAD_MUTEX_INITIALIZER;
static int texture_pin_hooks_ready;
static unsigned texture_pin_suspended, texture_pin_new, texture_pin_reused, texture_pin_rejected, texture_pin_released;
static void (*texture_pin_add_ref)(uintptr_t);
static void (*texture_pin_release_ref)(uintptr_t *);
static void pin_native_add(void *unused, uintptr_t object) {
    (void)unused; texture_pin_add_ref(object + 4);
}
static void pin_texture(uintptr_t object, const char *name, unsigned size) {
    if (!texture_pin_hooks_ready || !texture_pin_add_ref || !texture_pin_release_ref || !rio_texture_pin_name(name)) return;
    pthread_mutex_lock(&texture_pin_lock);
    if (!texture_pin_suspended) {
        int result = rio_texture_pin_add(&texture_pins, object, name,
            *(unsigned *)(object + 0x18), *(unsigned *)(object + 0x1c), size,
            RIO_TEXTURE_PIN_LIMIT, pin_native_add, NULL);
        if (result == 1) ++texture_pin_new;
        else if (result == 2) ++texture_pin_reused;
        else ++texture_pin_rejected;
    }
    pthread_mutex_unlock(&texture_pin_lock);
}
#ifdef GANGSTAR_TEXTURE_PREWARM
static int pin_return_cached(uintptr_t out, uintptr_t manager, const char *name) {
    if (!out || !texture_pin_hooks_ready || !gangstar_wait_is_main() ||
        atomic_load(&texture_prewarm_done) != 2 || manager != texture_prewarm_owner) return 0;
    pthread_mutex_lock(&texture_pin_lock);
    uintptr_t object = texture_pin_suspended ? 0 : rio_texture_pin_find(&texture_pins, name);
    if (object && (*(unsigned char *)(object + 0x38) & 0x88) == 0x88) {
        // Borrowing the pool pointer is insufficient: the hidden result owns
        // one native intrusive reference, exactly as the original getter does.
        pin_native_add(NULL, object); *(uintptr_t *)out = object; ++texture_pin_hits;
    } else object = 0;
    pthread_mutex_unlock(&texture_pin_lock);
    return object != 0;
}
static void pin_request(const char *op, uintptr_t manager, const char *name,
    const char *hash, unsigned mode, uintptr_t caller) {
    if (!gangstar_wait_is_main() || !rio_texture_pin_name(name)) return;
    if (texture_request_count == 48) { ++texture_request_dropped; return; }
    unsigned i = texture_request_count++;
    texture_requests[i].frame = reuse_frame; texture_requests[i].mode = mode;
    texture_requests[i].manager = manager; texture_requests[i].caller = caller;
    texture_requests[i].op = op;
    snprintf(texture_requests[i].name, 128, "%s", name);
    snprintf(texture_requests[i].hash, 128, "%s", hash ? hash : "(null)");
    pthread_mutex_lock(&texture_pin_lock);
    uintptr_t object = rio_texture_pin_find(&texture_pins, name);
    texture_requests[i].object = object;
    texture_requests[i].flags = object ? *(unsigned char *)(object + 0x38) : 0;
    pthread_mutex_unlock(&texture_pin_lock);
}
#endif
static void pin_suspend_and_clear(void) {
#ifdef GANGSTAR_TEXTURE_PREWARM
    atomic_store(&texture_prewarm_done, 0);
#endif
    pthread_mutex_lock(&texture_pin_lock); ++texture_pin_suspended;
    pthread_mutex_unlock(&texture_pin_lock);
    for (;;) {
        pthread_mutex_lock(&texture_pin_lock);
        uintptr_t object = rio_texture_pin_pop(&texture_pins);
        if (object) ++texture_pin_released;
        pthread_mutex_unlock(&texture_pin_lock);
        if (!object) break;
        // Native intrusive_ptr destructor handles its own refcount/manager logic.
        // Bookkeeping is already empty for this object before native reentrancy.
        texture_pin_release_ref(&object);
    }
}
static void pin_resume(void) {
    pthread_mutex_lock(&texture_pin_lock); --texture_pin_suspended;
    pthread_mutex_unlock(&texture_pin_lock);
}
static void pin_report(void) {
    pthread_mutex_lock(&texture_pin_lock);
    unsigned count = texture_pins.count, bytes = texture_pins.bytes, made = texture_pin_new;
    unsigned reused = texture_pin_reused, rejected = texture_pin_rejected, released = texture_pin_released;
    texture_pin_new = texture_pin_reused = texture_pin_rejected = texture_pin_released = 0;
    pthread_mutex_unlock(&texture_pin_lock);
#ifdef GANGSTAR_TEXTURE_PREWARM
    l_info("RIO_TEXTURE_CACHE hits=%u owner=0x%x phase=%d", texture_pin_hits,
        (unsigned)texture_prewarm_owner, atomic_load(&texture_prewarm_done));
    texture_pin_hits = 0;
    for (unsigned i = 0; i < texture_request_count; ++i)
        l_info("RIO_TEXTURE_REQUEST frame=%u op=%s manager=0x%x caller=0x%x mode=%u retained=0x%x flags=%u name=%s hash=%s",
            texture_requests[i].frame, texture_requests[i].op, (unsigned)texture_requests[i].manager,
            (unsigned)texture_requests[i].caller, texture_requests[i].mode, (unsigned)texture_requests[i].object,
            texture_requests[i].flags, texture_requests[i].name, texture_requests[i].hash);
    if (texture_request_dropped) l_info("RIO_TEXTURE_REQUEST dropped=%u", texture_request_dropped);
    texture_request_count = texture_request_dropped = 0;
#endif
    l_info("RIO_TEXTURE_PINS retained=%u estimated_bytes=%u new=%u seen_again=%u rejected=%u released=%u",
        count, bytes, made, reused, rejected, released);
}
#endif
static uintptr_t probe_21(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    uint64_t began = gangstar_wait_begin();
    GangstarAssetPerf before = began ? gangstar_asset_stats() : (GangstarAssetPerf){0};
    GangstarArchiveFrame io_before = began ? gangstar_archive_frame_stats() : (GangstarArchiveFrame){0};
    uintptr_t texture = began && c ? *(uintptr_t *)c : 0;
    uintptr_t result = probes[21].original(a, b, c, d);
    uint64_t elapsed = gangstar_wait_end(RIO_WAIT_NATIVE, began,
        (uintptr_t)__builtin_return_address(0), b, 0, (int)result, probes[21].label);
    if (began) {
        probes[21].us += elapsed; ++probes[21].calls;
        if (b) {
            // IReadFile remains owned by the caller across this native call.
            uintptr_t *vtable = *(uintptr_t **)b;
            unsigned size = ((unsigned (*)(uintptr_t))vtable[0x20 / 4])(b);
            const char *name = ((const char *(*)(uintptr_t))vtable[0x28 / 4])(b);
            record_asset_load(elapsed, size, name, probes[21].label);
            rio_texture_identity_note(&texture_identities, texture, name, reuse_frame);
#ifdef GANGSTAR_TEXTURE_REF_CACHE
            if (result && texture) pin_texture(texture, name, size);
#endif
            if (name && strnlen(name, 192) < 192) {
                unsigned h = 0;
                for (; h < reuse_history_count; ++h)
                    if (reuse_history[h].size == size && !strcmp(reuse_history[h].name, name)) break;
                if (h == reuse_history_count && h < 512) {
                    strcpy(reuse_history[h].name, name); reuse_history[h].size = size;
                    ++reuse_history_count;
                }
                unsigned occurrence = h < 512 ? ++reuse_history[h].seen : 0;
                if (reuse_count < 128) {
                    unsigned i = reuse_count++;
                    GangstarAssetPerf after = gangstar_asset_stats();
                    GangstarArchiveFrame io_after = gangstar_archive_frame_stats();
                    strcpy(reuse_events[i].name, name);
                    reuse_events[i].frame = reuse_frame; reuse_events[i].occurrence = occurrence;
                    reuse_events[i].texture = texture; reuse_events[i].size = size; reuse_events[i].us = elapsed;
                    reuse_events[i].calls = after.calls[RIO_ASSET_TEXTURE] - before.calls[RIO_ASSET_TEXTURE];
                    reuse_events[i].upload_us = after.us[RIO_ASSET_TEXTURE] - before.us[RIO_ASSET_TEXTURE];
                    reuse_events[i].read_us = io_after.elapsed_us - io_before.elapsed_us;
                    reuse_events[i].read_bytes = io_after.returned - io_before.returned;
                } else ++reuse_dropped;
            } else ++reuse_dropped;
        }
    }
    return result;
}
#ifdef GANGSTAR_RESIDENT_TEXTURES
static RioResidentPolicy resident_policy;
static uintptr_t (*resident_properties)(uintptr_t, unsigned);
static void (*resident_unbind)(uintptr_t, unsigned, unsigned);
static unsigned resident_kept, resident_released, resident_passes;
#endif
static uintptr_t probe_27(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
#ifdef GANGSTAR_RESIDENT_TEXTURES
    if (resident_properties && resident_unbind && gangstar_wait_is_main()) {
        uintptr_t first = *(uintptr_t *)(a + 0xbc), last = *(uintptr_t *)(a + 0xc0);
        uintptr_t entries = *(uintptr_t *)(a + 0x18), end = *(uintptr_t *)(a + 0x1c);
        if (last >= first && !((first | last | entries | end) & 3) &&
            last - first <= 65536 * 4 && (first || last == first) &&
            end >= entries && !((end - entries) & 7) && end - entries <= 65536 * 8 && (entries || end == entries)) {
            ++resident_passes;
            uint64_t free_vram = UINT64_MAX;
            // Original method only walks this vector and calls unbind(t,6,0).
            // Keep explicit unload/remove/shutdown paths completely native.
            for (uintptr_t at = first; at < last; at += 4) {
                uintptr_t texture = *(uintptr_t *)at;
                int keep = 0;
                if (texture) {
                    unsigned id = *(unsigned short *)(texture + 0x34);
                    if (id < (end - entries) / 8 && *(uintptr_t *)(entries + id * 8) == texture) {
                        uintptr_t props = resident_properties(a, id);
                        const char *name = props ? *(const char **)props : NULL;
                        if (rio_resident_name(name)) {
                        if (free_vram == UINT64_MAX) free_vram = vglMemFree(VGL_MEM_VRAM);
                        keep = rio_resident_keep(&resident_policy, a, texture, name,
                            *(unsigned *)(texture + 0x18), *(unsigned *)(texture + 0x1c),
                            (*(unsigned char *)(texture + 0x38) & 0x80) != 0,
                            (*(unsigned char *)(texture + 0x38) & 8) != 0, free_vram);
                        }
                    }
                }
                if (keep) ++resident_kept;
                else { ++resident_released; resident_unbind(texture, 6, 0); }
            }
            return 0;
        }
    }
#endif
    return probes[27].original(a, b, c, d);
}
static uintptr_t trace_texture_eviction(unsigned probe, unsigned kind, uintptr_t a,
    uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t caller) {
    uint64_t began = gangstar_wait_begin();
    uintptr_t object = kind == 1 ? (b ? *(uintptr_t *)b : 0) : kind == 2 ? b : a;
    unsigned serial = 0, slot = 64;
    if (began) {
        ++eviction_calls[kind];
        const RioTextureIdentity *id = rio_texture_identity_find(&texture_identities, object);
        if (id) {
            serial = id->serial;
            int relevant = rio_lod_name(id->name) &&
                (kind != 0 || (*(unsigned char *)(object + 0x38) & 8) != 0);
            if (!relevant) ++eviction_filtered;
            else if (eviction_count < 64) {
                slot = eviction_count++;
                strcpy(eviction_events[slot].name, id->name);
                eviction_events[slot].op = probes[probe].label;
                eviction_events[slot].object = object; eviction_events[slot].caller = caller;
                eviction_events[slot].frame = reuse_frame;
                eviction_events[slot].last_load_frame = id->frame;
                eviction_events[slot].load_serial = id->serial;
                eviction_events[slot].mask = kind == 0 ? b : 0;
                eviction_events[slot].ready = (*(unsigned char *)(object + 0x38) & 0x80) != 0;
                eviction_events[slot].bound = (*(unsigned char *)(object + 0x38) & 8) != 0;
            } else ++eviction_dropped;
        } else ++eviction_unknown[kind];
        // Forget before destruction; nested cleanup cannot dereference freed state.
        if (kind == 3 && serial) rio_texture_identity_forget(&texture_identities, object, serial);
    }
    uintptr_t result = probes[probe].original(a, b, c, d);
    uint64_t elapsed = gangstar_wait_end(RIO_WAIT_NATIVE, began, caller, object, 0,
        kind == 2 ? (int)result : 0, probes[probe].label);
    if (began) {
        probes[probe].us += elapsed; ++probes[probe].calls;
        if (slot < 64) {
            eviction_events[slot].us = elapsed;
            eviction_events[slot].removed = kind == 2 && result != 0;
        }
        if (kind == 2 && result && serial) rio_texture_identity_forget(&texture_identities, object, serial);
    }
    return result;
}
#define EVICT_PROBE(N, K) \
static __attribute__((noinline)) uintptr_t probe_##N(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) { \
    return trace_texture_eviction(N, K, a, b, c, d, (uintptr_t)__builtin_return_address(0)); \
}
EVICT_PROBE(28, 0) EVICT_PROBE(29, 1) EVICT_PROBE(30, 2) EVICT_PROBE(31, 3)
static pthread_mutex_t gpu_release_lock = PTHREAD_MUTEX_INITIALIZER;
static struct { uintptr_t object, caller; unsigned gl_name, flags, mask, frame; int thread;
    uint64_t at_us, elapsed_us;
} gpu_release_events[64];
static unsigned gpu_release_count, gpu_release_calls, gpu_release_empty, gpu_release_dropped;
static __attribute__((noinline)) uintptr_t probe_32(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    uint64_t began = sceKernelGetProcessTimeWide();
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    unsigned gl_name = *(unsigned *)(a + 0x60), flags = *(unsigned char *)(a + 0x38);
    int thread = sceKernelGetThreadId(), main = gangstar_wait_is_main();
    unsigned frame = main ? reuse_frame : 0;
    uintptr_t result = probes[32].original(a, b, c, d);
    uint64_t elapsed = sceKernelGetProcessTimeWide() - began;
    if (main) { probes[32].us += elapsed; ++probes[32].calls; }
    // Separate thread-safe raw queue: no identity lookups or post-release object reads.
    // Never hold this lock across native/GL calls.
    pthread_mutex_lock(&gpu_release_lock);
    ++gpu_release_calls;
    if (!gl_name) ++gpu_release_empty;
    else if (gpu_release_count < 64) {
        unsigned i = gpu_release_count++;
        gpu_release_events[i].object = a; gpu_release_events[i].caller = caller;
        gpu_release_events[i].gl_name = gl_name; gpu_release_events[i].flags = flags;
        gpu_release_events[i].mask = b; gpu_release_events[i].frame = frame;
        gpu_release_events[i].thread = thread; gpu_release_events[i].at_us = began;
        gpu_release_events[i].elapsed_us = elapsed;
    } else ++gpu_release_dropped;
    pthread_mutex_unlock(&gpu_release_lock);
    return result;
}
static uintptr_t pin_clear_call(unsigned probe, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
#ifdef GANGSTAR_TEXTURE_REF_CACHE
    pin_suspend_and_clear();
#endif
    uintptr_t result = probes[probe].original(a, b, c, d);
#ifdef GANGSTAR_TEXTURE_REF_CACHE
    pin_resume();
#endif
    return result;
}
static uintptr_t probe_33(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) { return pin_clear_call(33,a,b,c,d); }
static uintptr_t probe_34(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) { return pin_clear_call(34,a,b,c,d); }
#ifdef GANGSTAR_WORLD_SOURCE_PREWARM
static void preload_world_sources(uintptr_t manager) {
    if(gangstar_district_start())return;
    if (world_source_preload_done || !world_source_fs_vtable || !world_source_file_release) return;
    // The native getter uses this exact intrusive IFileSystem member. Opening
    // through its virtual API retains the engine's filesystem locking, archive
    // priority, reduced-file construction and canonical filename handling.
    uintptr_t fs=*(uintptr_t *)(manager+0x60);
    if (!fs || *(uintptr_t *)fs!=world_source_fs_vtable) {
        l_warn("RIO_SOURCE_PREWARM skipped: unsupported texture filesystem"); return;
    }
    uintptr_t *vtable=*(uintptr_t **)fs;
    NativeCall open=(NativeCall)vtable[0x0c/4];
    uint64_t began=sceKernelGetProcessTimeWide();
    unsigned ready=0, missing=0, original=0, expected_bytes=0, requested=0;
    const unsigned candidates=sizeof(rio_world_texture_names)/sizeof(*rio_world_texture_names);
    gangstar_texture_source_preloading(1);
    for (unsigned i=0;i<candidates;++i) {
        if (!rio_lod_preload_source(rio_world_texture_names[i])) continue;
        ++requested;
        char path[192]; uintptr_t file=0;
        unsigned expected=rio_lod_original_source(rio_world_texture_names[i]) ? 2796344u : rio_world_texture_bytes[i];
        expected_bytes+=expected;
        snprintf(path,sizeof(path),"_packetc/%s",rio_world_texture_names[i]);
        open((uintptr_t)&file,fs,(uintptr_t)path,0);
        if (!file) { ++missing; continue; }
        uintptr_t *file_vtable=*(uintptr_t **)file;
        unsigned size=((unsigned (*)(uintptr_t))file_vtable[0x20/4])(file);
        if (size==expected) ++ready; else ++original;
        // Native release takes the object, NOT the address of the local ref.
        // It frees the owned reader; a separate cache copy remains if budget permits.
        world_source_file_release(file);
    }
    gangstar_texture_source_preloading(0);
    // Keep the generic preload intact. Supplemental full-size lightmaps use
    // the spare-only demand policy; a full cache opens the native reader
    // without loading its payload here. No gameplay background I/O is added.
    unsigned supplemental_opened=0;
    for(unsigned i=0;i<sizeof(rio_lod_supplemental_sources)/sizeof(*rio_lod_supplemental_sources);++i) {
        char path[192];uintptr_t file=0;
        snprintf(path,sizeof(path),"_packetc/%s",rio_lod_supplemental_sources[i]);
        open((uintptr_t)&file,fs,(uintptr_t)path,0);
        if(file){++supplemental_opened;world_source_file_release(file);}
    }
    l_info("RIO_SOURCE_SUPPLEMENTAL attempted=%u opened=%u spare_only=1",
        (unsigned)(sizeof(rio_lod_supplemental_sources)/sizeof(*rio_lod_supplemental_sources)),supplemental_opened);
    world_source_preload_done=ready==requested;
    l_info("RIO_SOURCE_PREWARM requested=%u ready=%u missing=%u original=%u expected_bytes=%u elapsed_us=%llu",
        requested,ready,missing,original,expected_bytes,
        (unsigned long long)(sceKernelGetProcessTimeWide()-began));
    gangstar_texture_lod_report(reuse_frame);
}
#endif
static uintptr_t probe_35(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    uintptr_t result = probes[35].original(a,b,c,d);
#if defined(GANGSTAR_TEXTURE_PREWARM) && defined(GANGSTAR_TEXTURE_REF_CACHE)
    // Original streamingInit deactivates/locks HUD before this native loading phase.
    if (texture_pin_hooks_ready && texture_prewarm_manager && texture_prewarm_get && gangstar_wait_is_main()) {
        uintptr_t manager = texture_prewarm_manager();
        int expected_phase = 0;
        if (manager && atomic_compare_exchange_strong(&texture_prewarm_done, &expected_phase, 1)) {
            uint64_t began = sceKernelGetProcessTimeWide();
            unsigned returned = 0, ready = 0;
            texture_prewarm_owner = manager;
#ifdef GANGSTAR_WORLD_SOURCE_PREWARM
            preload_world_sources(manager);
#endif
            for (unsigned i = 0; i < RIO_TEXTURE_PIN_TARGETS; ++i) {
                uintptr_t texture = 0;
                char canonical[128];
                snprintf(canonical, sizeof(canonical), "_packetc/%s", rio_texture_pin_names[i]);
                // addTexture uses IReadFile's canonical filename as its key.
                // Use that same namespace for lookup and creation.
                texture_prewarm_get((uintptr_t)&texture, manager, (uintptr_t)canonical, (uintptr_t)canonical);
                if (texture) {
                    ++returned; ready += (*(unsigned char *)(texture + 0x38) & 0x80) != 0;
                    texture_pin_release_ref(&texture);
                }
            }
            atomic_store(&texture_prewarm_done, 2);
            l_info("RIO_PREWARM frame=%u requested=%u returned=%u ready_at_return=%u elapsed_us=%llu",
                reuse_frame, (unsigned)RIO_TEXTURE_PIN_TARGETS, returned, ready,
                (unsigned long long)(sceKernelGetProcessTimeWide() - began));
        }
    }
#endif
    return result;
}
static uintptr_t probe_36(uintptr_t out, uintptr_t manager, uintptr_t name, uintptr_t hash) {
#if defined(GANGSTAR_TEXTURE_PREWARM) && defined(GANGSTAR_TEXTURE_REF_CACHE)
    pin_request("name", manager, (const char *)name, (const char *)hash, 0, (uintptr_t)__builtin_return_address(0));
    if (rio_texture_pin_key_matches((const char *)name, (const char *)hash) &&
        pin_return_cached(out, manager, (const char *)name)) return out;
#endif
    return probes[36].original(out, manager, name, hash);
}
// This overload has a fifth stack argument. Keep its real ABI on BOTH paths.
static uintptr_t probe_37(uintptr_t out, uintptr_t manager, uintptr_t file_ref, uintptr_t hash, uintptr_t mode) {
#if defined(GANGSTAR_TEXTURE_PREWARM) && defined(GANGSTAR_TEXTURE_REF_CACHE)
    if (file_ref && *(uintptr_t *)file_ref && gangstar_wait_is_main()) {
        uintptr_t file = *(uintptr_t *)file_ref;
        uintptr_t *vtable = *(uintptr_t **)file;
        const char *name = ((const char *(*)(uintptr_t))vtable[0x28 / 4])(file);
        pin_request("file", manager, name, (const char *)hash, mode, (uintptr_t)__builtin_return_address(0));
        if (!mode && rio_texture_pin_key_matches(name, (const char *)hash) &&
            pin_return_cached(out, manager, name)) return out;
    }
#endif
    typedef uintptr_t (*GetFile)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    return ((GetFile)probes[37].original)(out, manager, file_ref, hash, mode);
}
// Seven-argument native overload: hidden return, manager, file, string,
// load mode and two optional recreation references. Preserve all stack args.
static uintptr_t probe_38(uintptr_t out, uintptr_t manager, uintptr_t file, uintptr_t hash_ref,
    uintptr_t mode, uintptr_t replacement, uintptr_t alpha_replacement) {
#if defined(GANGSTAR_TEXTURE_PREWARM) && defined(GANGSTAR_TEXTURE_REF_CACHE)
    if (file && gangstar_wait_is_main()) {
        uintptr_t *vtable = *(uintptr_t **)file;
        const char *name = ((const char *(*)(uintptr_t))vtable[0x28 / 4])(file);
        const char *hash = hash_ref ? *(const char **)hash_ref : NULL;
        unsigned overrides = (replacement && *(uintptr_t *)replacement) ||
            (alpha_replacement && *(uintptr_t *)alpha_replacement);
        pin_request("internal", manager, name, hash, mode | (overrides ? 256u : 0u),
            (uintptr_t)__builtin_return_address(0));
        if (!mode && !overrides && rio_texture_pin_key_matches(name, hash) &&
            pin_return_cached(out, manager, name)) return out;
    }
#endif
    typedef uintptr_t (*GetInternal)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    return ((GetInternal)probes[38].original)(out, manager, file, hash_ref, mode, replacement, alpha_replacement);
}
#ifdef GANGSTAR_AUDIO_SCOPED_PRIORITY
static atomic_uint audio_sched_boosted,audio_sched_restored,audio_sched_begin_errors,audio_sched_restore_errors;
static int audio_priority_get(void *context) {
    SceKernelThreadInfo info={0};info.size=sizeof(info);
    int result=sceKernelGetThreadInfo((SceUID)(uintptr_t)context,&info);
    return result<0?result:info.currentPriority;
}
static int audio_priority_set(void *context,int priority) {
    return sceKernelChangeThreadPriority((SceUID)(uintptr_t)context,priority);
}
#endif
static __attribute__((noinline)) uintptr_t probe_39(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {
    SceUID thread=sceKernelGetThreadId();
    if (!gangstar_wait_is_main()) gangstar_audio_poll_worker();
#ifdef GANGSTAR_AUDIO_SCOPED_PRIORITY
    RioAudioScheduleOps scheduling={audio_priority_get,audio_priority_set,(void *)(uintptr_t)thread};
    RioAudioSchedule priority=rio_audio_schedule_begin(&scheduling);
    if(priority.error<0)atomic_fetch_add(&audio_sched_begin_errors,1);
    if(priority.changed)atomic_fetch_add(&audio_sched_boosted,1);
#endif
    SceKernelThreadInfo before={0},after={0};before.size=sizeof(before);after.size=sizeof(after);
    int before_ok=sceKernelGetThreadInfo(thread,&before);
    RioAudioProbeScope scope;
    rio_audio_scope_begin(&scope,a,thread,sceKernelGetProcessTimeWide());
    uintptr_t result=probes[39].original(a,b,c,d);
    uint64_t ended=sceKernelGetProcessTimeWide();
    int after_ok=sceKernelGetThreadInfo(thread,&after);
    if(before_ok>=0 && after_ok>=0 && after.runClocks>=before.runClocks)
        rio_audio_scheduler(after.runClocks-before.runClocks,
            after.threadPreemptCount-before.threadPreemptCount,
            after.threadReleaseCount-before.threadReleaseCount,
            (unsigned)after.changeCpuCount-(unsigned)before.changeCpuCount);
#ifdef GANGSTAR_AUDIO_SCOPED_PRIORITY
    int changed=priority.changed;
    int restored=rio_audio_schedule_end(&priority,&scheduling);
    if(changed)atomic_fetch_add(restored<0?&audio_sched_restore_errors:&audio_sched_restored,1);
#endif
    rio_audio_scope_end(&scope,ended);
    return result;
}
#define AUDIO_PROBE(N,PHASE) \
static __attribute__((noinline)) uintptr_t probe_##N(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) { \
    uint64_t began=rio_audio_scope_active()?sceKernelGetProcessTimeWide():0; \
    uintptr_t result=probes[N].original(a,b,c,d); \
    if(began)rio_audio_phase(PHASE,sceKernelGetProcessTimeWide()-began); \
    return result; \
}
AUDIO_PROBE(40,RIO_AUDIO_LOAD_ASYNC)
AUDIO_PROBE(41,RIO_AUDIO_DSP)
#ifdef GANGSTAR_AUDIO_HALF_CHUNKS
static uintptr_t chunk_fill,chunk_submit,chunk_cursor_vtable,chunk_ima_vtable;
static atomic_uint chunk_half_calls,chunk_original_calls;
#endif
static __attribute__((noinline)) uintptr_t probe_42(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {
    uint64_t began=rio_audio_scope_active()?sceKernelGetProcessTimeWide():0;
    unsigned original=0,requested=0;
    if(began) {
        uintptr_t driver=*(uintptr_t *)(a+0x110),decoder=*(uintptr_t *)(a+0x114);
        uintptr_t submit=driver?*(uintptr_t *)(*(uintptr_t *)driver+0x28):0;
        uintptr_t fill=decoder?*(uintptr_t *)(*(uintptr_t *)decoder+0x14):0;
        original=requested=*(unsigned *)(a+0x108);
        unsigned buffers=*(unsigned *)(a+0x104);
#ifdef GANGSTAR_AUDIO_HALF_CHUNKS
        int known=0;
        if(chunk_fill && chunk_submit && chunk_cursor_vtable && chunk_ima_vtable &&
            fill==chunk_fill && submit==chunk_submit && decoder &&
            *(uintptr_t *)decoder==chunk_cursor_vtable) {
            uintptr_t sub=*(uintptr_t *)(decoder+0x24);
            known=sub && *(uintptr_t *)sub==chunk_ima_vtable;
            if(known)rio_audio_source(*(unsigned *)(sub+0x1c),*(unsigned *)(sub+0x14),*(unsigned *)(sub+0x10));
        }
        requested=rio_audio_half_chunk(original,buffers,known,rio_audio_scope_matches(a));
        if(requested!=original) {
            *(unsigned *)(a+0x108)=requested;
            atomic_fetch_add(&chunk_half_calls,1);
        } else atomic_fetch_add(&chunk_original_calls,1);
#endif
        rio_audio_stream(fill,submit,requested,buffers);
    }
    uintptr_t result=probes[42].original(a,b,c,d);
    if(requested!=original)*(unsigned *)(a+0x108)=original;
    if(began)rio_audio_phase(RIO_AUDIO_PROCESS,sceKernelGetProcessTimeWide()-began);
    return result;
}
AUDIO_PROBE(43,RIO_AUDIO_MPC_DECODE)
AUDIO_PROBE(44,RIO_AUDIO_WAV_DECODE)
AUDIO_PROBE(45,RIO_AUDIO_NATIVE_DECODE)
AUDIO_PROBE(46,RIO_AUDIO_VORBIS_DECODE)
#ifdef GANGSTAR_NATIVE_ADPCM_VERIFY
static struct { atomic_uint checked,matched,mismatch,unsupported,native_us,original_us; } adpcm_verify[2];
#ifdef GANGSTAR_NATIVE_IMA_FAST
static atomic_uint ima_fast_blocks,ima_fast_fallback,ima_fast_errors,ima_fast_us;
static atomic_int ima_fast_disabled;
struct NativeImaStream { uintptr_t object,vtable; };
static int native_ima_tell(void *p) {
    struct NativeImaStream *s=p;typedef int (*Tell)(uintptr_t);
    return ((Tell)*(uintptr_t *)(s->vtable+0x18))(s->object);
}
static int native_ima_read(void *p,void *data,unsigned bytes) {
    struct NativeImaStream *s=p;typedef int (*Read)(uintptr_t,void *,int);
    return ((Read)*(uintptr_t *)(s->vtable+0x1c))(s->object,data,(int)bytes);
}
static int native_ima_seek(void *p,int position) {
    struct NativeImaStream *s=p;typedef int (*Seek)(uintptr_t,int,int);
    return ((Seek)*(uintptr_t *)(s->vtable+0x10))(s->object,position,0);
}
static uintptr_t fast_ima_block(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {
    uintptr_t wave=*(uintptr_t *)(a+8),stream=*(uintptr_t *)(a+4);
    unsigned channels=*(unsigned *)(a+0x10);
    if(atomic_load(&ima_fast_disabled) || !wave || !stream || *(uint16_t *)(wave+0x16)!=channels)goto fallback;
    RioImaBlock block={.channels=channels,.block_bytes=*(uint16_t *)(wave+0x20),
        .position=*(unsigned *)(a+0x58),.data_bytes=*(unsigned *)(wave+0x28),
        .completed=*(unsigned *)(a+0x68),.total_frames=*(unsigned *)(a+0x1c),
        .scratch=*(uint8_t **)(a+0x6c)};
    struct NativeImaStream context={stream,*(uintptr_t *)stream};
    RioImaStream callbacks={native_ima_tell,native_ima_read,native_ima_seek,&context};
    uint64_t began=sceKernelGetProcessTimeWide();
    int frames=rio_ima_read_block(&block,&callbacks,(int16_t *)b,2048);
    if(frames==-1)goto fallback;
    if(frames==-2) { atomic_store(&ima_fast_disabled,1);atomic_fetch_add(&ima_fast_errors,1);return 0; }
    if(block.read_error)atomic_fetch_add(&ima_fast_errors,1);
    /* Only fields written by the original DecodeBlock; surrounding native
       Decode/Seek retains ownership of ring indices, looping and frame counts. */
    if(block.position!=*(unsigned *)(a+0x58)) {
        memcpy((void *)(a+0x2a),block.state,channels*4);
        *(unsigned *)(a+0x58)=block.position;
        atomic_fetch_add(&ima_fast_blocks,1);
        atomic_fetch_add(&ima_fast_us,(unsigned)(sceKernelGetProcessTimeWide()-began));
    }
    return (uintptr_t)frames;
fallback:
    atomic_fetch_add(&ima_fast_fallback,1);
    return probes[47].original(a,b,c,d);
}
#endif
static uintptr_t verify_adpcm_block(unsigned kind,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {
    unsigned position=kind?0x30:0x58;
    unsigned before=*(unsigned *)(a+position);
    uint64_t began=sceKernelGetProcessTimeWide();
    uintptr_t result=probes[47+kind].original(a,b,c,d);
    unsigned original_us=(unsigned)(sceKernelGetProcessTimeWide()-began);
    if((int)result<=0)return result;
    unsigned bytes=*(unsigned *)(a+position)-before,channels=*(unsigned *)(a+0x10);
    if(bytes>1024 || channels<1 || channels>2 || result>2048/channels ||
        (kind && *(uint16_t *)(a+0x4e)!=7)) {
        atomic_fetch_add(&adpcm_verify[kind].unsupported,1);return result;
    }
    const uint8_t *input=*(const uint8_t **)(a+(kind?0x44:0x6c));
    int16_t pcm[2048];int16_t coefficients[14];
    if(kind)memcpy(coefficients,(void *)(a+0x50),sizeof(coefficients));
    began=sceKernelGetProcessTimeWide();
    size_t frames=kind?rio_adpcm_ms(input,bytes,channels,coefficients,pcm,2048):
        rio_adpcm_ima(input,bytes,channels,pcm,2048);
    atomic_fetch_add(&adpcm_verify[kind].native_us,(unsigned)(sceKernelGetProcessTimeWide()-began));
    atomic_fetch_add(&adpcm_verify[kind].original_us,original_us);
    if(!frames) { atomic_fetch_add(&adpcm_verify[kind].unsupported,1);return result; }
    atomic_fetch_add(&adpcm_verify[kind].checked,1);
    int match=frames>=result && !memcmp(pcm,(void *)b,result*channels*sizeof(int16_t));
    atomic_fetch_add(match?&adpcm_verify[kind].matched:&adpcm_verify[kind].mismatch,1);
    return result; /* Playback always uses the original PCM and original state. */
}
static uintptr_t probe_47(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {
#ifdef GANGSTAR_NATIVE_IMA_FAST
    return fast_ima_block(a,b,c,d);
#else
    return verify_adpcm_block(0,a,b,c,d);
#endif
}
static uintptr_t probe_48(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) { return verify_adpcm_block(1,a,b,c,d); }
AUDIO_PROBE(49,RIO_AUDIO_CURSOR_SEEK)
#endif
static void audio_probe_report(void) {
#ifdef GANGSTAR_AUDIO_HALF_CHUNKS
    l_info("RIO_AUDIO_CHUNK half_calls=%u original_calls=%u",atomic_exchange(&chunk_half_calls,0),atomic_exchange(&chunk_original_calls,0));
#endif
#ifdef GANGSTAR_NATIVE_IMA_FAST
    l_info("RIO_ADPCM_FAST codec=ima blocks=%u fallback=%u errors=%u total_us=%u disabled=%d",
        atomic_exchange(&ima_fast_blocks,0),atomic_exchange(&ima_fast_fallback,0),
        atomic_exchange(&ima_fast_errors,0),atomic_exchange(&ima_fast_us,0),atomic_load(&ima_fast_disabled));
#endif
#ifdef GANGSTAR_NATIVE_ADPCM_VERIFY
    for(unsigned i=0;i<2;++i)
        l_info("RIO_ADPCM_VERIFY codec=%s checked=%u matched=%u mismatch=%u unsupported=%u native_math_us=%u original_block_us=%u",
            i?"ms":"ima",atomic_exchange(&adpcm_verify[i].checked,0),atomic_exchange(&adpcm_verify[i].matched,0),
            atomic_exchange(&adpcm_verify[i].mismatch,0),atomic_exchange(&adpcm_verify[i].unsupported,0),
            atomic_exchange(&adpcm_verify[i].native_us,0),atomic_exchange(&adpcm_verify[i].original_us,0));
#endif
#ifdef GANGSTAR_AUDIO_SCOPED_PRIORITY
    l_info("RIO_AUDIO_SCHED boosted=%u restored=%u begin_errors=%u restore_errors=%u target_priority=160 original_priority=191",
        atomic_exchange(&audio_sched_boosted,0),atomic_exchange(&audio_sched_restored,0),
        atomic_exchange(&audio_sched_begin_errors,0),atomic_exchange(&audio_sched_restore_errors,0));
#endif
    RioAudioProbeEvent events[64];unsigned dropped=0;
    unsigned count=rio_audio_drain(events,64,&dropped);
    for(unsigned i=0;i<count;++i) {
        RioAudioProbeEvent *e=&events[i];
        l_info("RIO_EMITTER_UPDATE thread=%d object=0x%x started_us=%llu elapsed_us=%llu load_async_us=%llu dsp_us=%llu process_us=%llu file_us=%llu raw_us=%llu ram_us=%llu convert_us=%llu mutex_us=%llu read_us=%llu",
            e->thread,(unsigned)e->object,(unsigned long long)e->started,(unsigned long long)e->elapsed,
            (unsigned long long)e->us[0],(unsigned long long)e->us[1],(unsigned long long)e->us[2],
            (unsigned long long)e->us[3],(unsigned long long)e->us[4],(unsigned long long)e->us[5],
            (unsigned long long)e->us[6],(unsigned long long)e->us[7],(unsigned long long)e->us[8]);
        l_info("RIO_EMITTER_STREAM object=0x%x started_us=%llu fill=0x%x submit=0x%x chunk=%u buffers=%u mpc_us=%llu wav_us=%llu native_us=%llu vorbis_us=%llu",
            (unsigned)e->object,(unsigned long long)e->started,(unsigned)e->decoder_target,
            (unsigned)e->submit_target,e->chunk,e->buffers,
            (unsigned long long)e->us[9],(unsigned long long)e->us[10],
            (unsigned long long)e->us[11],(unsigned long long)e->us[12]);
        l_info("RIO_EMITTER_IO object=0x%x started_us=%llu cache_us=%llu cache_calls=%u disk_us=%llu disk_calls=%u position_query_us=%llu position_set_us=%llu",
            (unsigned)e->object,(unsigned long long)e->started,
            (unsigned long long)e->us[RIO_AUDIO_CACHE],e->calls[RIO_AUDIO_CACHE],
            (unsigned long long)e->us[RIO_AUDIO_DISK],e->calls[RIO_AUDIO_DISK],
            (unsigned long long)e->us[RIO_AUDIO_POSITION_QUERY],(unsigned long long)e->us[RIO_AUDIO_POSITION_SET]);
        l_info("RIO_EMITTER_COST object=0x%x started_us=%llu seek_us=%llu seek_calls=%u tell_us=%llu tell_calls=%u cursor_seek_us=%llu cursor_seek_calls=%u source_frames=%u rate=%u channels=%u cpu_valid=%d run_clocks=%llu preemptions=%u releases=%u migrations=%u",
            (unsigned)e->object,(unsigned long long)e->started,(unsigned long long)e->us[13],e->calls[13],
            (unsigned long long)e->us[14],e->calls[14],(unsigned long long)e->us[15],e->calls[15],
            e->source_frames,e->source_rate,e->source_channels,e->cpu_valid,
            (unsigned long long)e->run_clocks,e->preemptions,e->releases,e->migrations);
    }
    if(dropped)l_info("RIO_EMITTER_UPDATE dropped=%u",dropped);
}
#ifdef GANGSTAR_NATIVE_ADPCM_VERIFY
#define PHASE_BASE 50
#else
#define PHASE_BASE 47
#endif
static RioPhaseStack phase_stack;
static RioRegistrationProfile registration_profile;
static unsigned cull_prepare_depth;
static struct {uint64_t calls,us,worst;unsigned samples;uint32_t random;} cull_lookup[4]={{.random=0x7ab451c3u},{.random=0x31502fadu},{.random=0x65c981b7u},{.random=0xa1d45763u}};
enum { RIO_PHASE_REGISTER_TREE=35, RIO_PHASE_CULL_PREPARE=39, RIO_PHASE_LOOKUP_BASE=42, RIO_PHASE_NATIVE_TID=46, RIO_PHASE_SUBMIT_BASE=47, RIO_PHASE_COUNT=55 };
static RioPhaseTotals phase_window[RIO_PHASE_COUNT];
static struct {uint64_t calls,us;unsigned samples;uint32_t random;} submit_sample[8]={{.random=0x634128afu},{.random=0xa1345f71u},{.random=0x619f341bu},{.random=0xa312f75du},{.random=0x71365fadu},{.random=0xc31572abu},{.random=0x84732ab1u},{.random=0x6734adefu}};
// Only the frame thread uses this slot, reset at the start of each frame.
// The first native lookup still initializes/reads real TLS. Every worker
// lookup continues through the original implementation.
static struct {uintptr_t value;unsigned valid;uint64_t hits,fills,outside_hits;} frame_thread_id;
static void sequential_id_frame_begin(void){frame_thread_id.valid=0;}
static uintptr_t sequential_id_call(NativeCall original,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {
 if(!gangstar_wait_is_main())return original(a,b,c,d);
 if(frame_thread_id.valid){++frame_thread_id.hits;if(!cull_prepare_depth)++frame_thread_id.outside_hits;return frame_thread_id.value;}
 uintptr_t result=original(a,b,c,d);++frame_thread_id.fills;
 if(result){frame_thread_id.value=result;frame_thread_id.valid=1;}
 return result;
}
static int native_probe_needed(unsigned i) {
#ifdef GANGSTAR_DETAILED_RENDER_PROFILE
 (void)i;return 1;
#else
 // Early wrappers also implement archive/audio/preload fixes. Keep those
 // and the native thread-ID cache; omit timing-only engine/lock hooks.
 return i<PHASE_BASE || i==PHASE_BASE+RIO_PHASE_NATIVE_TID || i>=PHASE_BASE+RIO_PHASE_SUBMIT_BASE;
#endif
}
// Borrow only while the source material remains alive inside cloneHeap.
// The destination receives its own atomic reference, matching native get().
static uintptr_t clone_name_data;
static uint64_t clone_name_hits;
static uintptr_t submit_native_call(unsigned kind,NativeCall original,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {
 if(kind==3) {
  uintptr_t previous=clone_name_data;
  clone_name_data=(!b && a)?*(uintptr_t *)(a+28):0;
  uintptr_t result=original(a,b,c,d);
  clone_name_data=previous;return result;
 }
 if(kind==7 && b==1 && clone_name_data && a==clone_name_data+4) {
  __atomic_fetch_add((unsigned *)clone_name_data,1u,__ATOMIC_SEQ_CST);
  ++clone_name_hits;return clone_name_data;
 }
 return original(a,b,c,d);
}
static uintptr_t phase_call(unsigned phase,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {
 struct Probe *p=&probes[PHASE_BASE+phase];
 if(phase==RIO_PHASE_NATIVE_TID)return sequential_id_call(p->original,a,b,c,d);
 if(!gangstar_wait_is_main())return p->original(a,b,c,d);
 if(phase>=RIO_PHASE_SUBMIT_BASE) {
  unsigned k=phase-RIO_PHASE_SUBMIT_BASE;++submit_sample[k].calls;
  if(!rio_render_choose(&submit_sample[k].random))return submit_native_call(k,p->original,a,b,c,d);
  uint64_t began=sceKernelGetProcessTimeWide();
  uintptr_t result=submit_native_call(k,p->original,a,b,c,d);
  submit_sample[k].us+=sceKernelGetProcessTimeWide()-began;++submit_sample[k].samples;
  return result;
 }

 if(phase>=RIO_PHASE_LOOKUP_BASE) {
  unsigned kind=phase-RIO_PHASE_LOOKUP_BASE;
  if(!cull_prepare_depth)return p->original(a,b,c,d);
  ++cull_lookup[kind].calls;
  if(!rio_render_choose(&cull_lookup[kind].random))return p->original(a,b,c,d);
  uint64_t began=sceKernelGetProcessTimeWide();
  uintptr_t result=p->original(a,b,c,d);
  uint64_t elapsed=sceKernelGetProcessTimeWide()-began;
  ++cull_lookup[kind].samples;cull_lookup[kind].us+=elapsed;
  if(elapsed>cull_lookup[kind].worst)cull_lookup[kind].worst=elapsed;
  return result;
 }
 // Snapshot identities before native code can release the referenced scene.
 // These offsets are the same camera/culler/root fields read by the verified
 // registerSceneNodes prologue/body. Metadata only; never reused for drawing.
 uintptr_t register_root=0,register_camera=0,register_culler=0;
 if(phase==RIO_PHASE_REGISTER_TREE && a && b) {
  register_camera=*(uintptr_t *)(a+0xd8);
  if(register_camera) {
   register_culler=*(uintptr_t *)(a+0x28);
   if(register_culler)register_root=*(uintptr_t *)b;
  }
 }
 int slot=rio_phase_enter(&phase_stack);
 if(phase==RIO_PHASE_CULL_PREPARE)++cull_prepare_depth;
 uint64_t began=sceKernelGetProcessTimeWide();
 uintptr_t result=p->original(a,b,c,d);
 uint64_t elapsed=sceKernelGetProcessTimeWide()-began;
 if(phase==RIO_PHASE_CULL_PREPARE)--cull_prepare_depth;
 rio_phase_leave(&phase_stack,&phase_window[phase],slot,elapsed);
 if(phase==RIO_PHASE_REGISTER_TREE)rio_registration_record(&registration_profile,a,register_root,register_camera,register_culler,elapsed);
 p->us+=elapsed;++p->calls;
 return result;
}
#define PHASE_WRAPPER(N) static uintptr_t phase_##N(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {return phase_call(N,a,b,c,d);}
PHASE_WRAPPER(0) PHASE_WRAPPER(1) PHASE_WRAPPER(2) PHASE_WRAPPER(3) PHASE_WRAPPER(4)
PHASE_WRAPPER(5) PHASE_WRAPPER(6) PHASE_WRAPPER(7) PHASE_WRAPPER(8)
PHASE_WRAPPER(9) PHASE_WRAPPER(10) PHASE_WRAPPER(11) PHASE_WRAPPER(12) PHASE_WRAPPER(13) PHASE_WRAPPER(14) PHASE_WRAPPER(15)
PHASE_WRAPPER(16) PHASE_WRAPPER(17) PHASE_WRAPPER(18) PHASE_WRAPPER(19) PHASE_WRAPPER(20) PHASE_WRAPPER(21)
PHASE_WRAPPER(22) PHASE_WRAPPER(23) PHASE_WRAPPER(24) PHASE_WRAPPER(25) PHASE_WRAPPER(26) PHASE_WRAPPER(27) PHASE_WRAPPER(28) PHASE_WRAPPER(29)
PHASE_WRAPPER(30) PHASE_WRAPPER(31) PHASE_WRAPPER(32) PHASE_WRAPPER(33) PHASE_WRAPPER(34) PHASE_WRAPPER(35) PHASE_WRAPPER(36) PHASE_WRAPPER(37) PHASE_WRAPPER(38) PHASE_WRAPPER(39) PHASE_WRAPPER(40) PHASE_WRAPPER(41) PHASE_WRAPPER(42) PHASE_WRAPPER(43) PHASE_WRAPPER(44) PHASE_WRAPPER(45) PHASE_WRAPPER(46) PHASE_WRAPPER(47) PHASE_WRAPPER(48) PHASE_WRAPPER(49) PHASE_WRAPPER(50) PHASE_WRAPPER(51) PHASE_WRAPPER(52) PHASE_WRAPPER(53) PHASE_WRAPPER(54)
static NativeCall wrappers[] = {probe_0, probe_1, probe_2, probe_3, probe_4, probe_5, probe_6, probe_7,
    probe_8, probe_9, probe_10, probe_11, probe_12, probe_13, probe_14, probe_15, probe_16, probe_17, probe_18, probe_19, probe_20, probe_21, probe_22, probe_23, probe_24, probe_25, probe_26, probe_27, probe_28, probe_29, probe_30, probe_31, probe_32, probe_33, probe_34, probe_35, probe_36, (NativeCall)probe_37, (NativeCall)probe_38,probe_39,probe_40,probe_41,probe_42,probe_43,probe_44,probe_45,probe_46
#ifdef GANGSTAR_NATIVE_ADPCM_VERIFY
    ,probe_47,probe_48,probe_49
#endif
    ,phase_0,phase_1,phase_2,phase_3,phase_4,phase_5,phase_6,phase_7,phase_8,phase_9,phase_10,phase_11,phase_12,phase_13,phase_14,phase_15,phase_16,phase_17,phase_18,phase_19,phase_20,phase_21,phase_22,phase_23,phase_24,phase_25,phase_26,phase_27,phase_28,phase_29,phase_30,phase_31,phase_32,phase_33,phase_34,phase_35,phase_36,phase_37,phase_38,phase_39,phase_40,phase_41,phase_42,phase_43,phase_44,phase_45,phase_46,phase_47,phase_48,phase_49,phase_50,phase_51,phase_52,phase_53,phase_54
};
_Static_assert(sizeof(wrappers)/sizeof(*wrappers)==sizeof(probes)/sizeof(*probes), "Native probe wrapper count");
#endif

void gangstar_native_phase_window_report(void) {
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
 gangstar_render_report();
 l_info("RIO_CLONE_NAME reused_native_refs=%llu",(unsigned long long)clone_name_hits);clone_name_hits=0;
 static const char *submit_names[]={"material","mesh","material_internal","material_clone","process_alloc","material_drop","material_init","shared_string"};
 for(unsigned k=0;k<8;++k) {
  l_info("RIO_SUBMIT_SAMPLE kind=%s calls=%llu samples=%u sampled_us=%llu",submit_names[k],
   (unsigned long long)submit_sample[k].calls,submit_sample[k].samples,(unsigned long long)submit_sample[k].us);
  submit_sample[k].calls=submit_sample[k].us=0;submit_sample[k].samples=0;
 }

#ifndef GANGSTAR_DETAILED_RENDER_PROFILE
 l_info("RIO_THREAD_ID_CACHE scope=frame_main hits=%llu native_fills=%llu",
  (unsigned long long)frame_thread_id.hits,(unsigned long long)frame_thread_id.fills);
 frame_thread_id.hits=frame_thread_id.fills=frame_thread_id.outside_hits=0;
 return;
#endif
 l_info("RIO_THREAD_ID_CACHE scope=frame_main hits=%llu native_fills=%llu outside_cull_hits=%llu",
  (unsigned long long)frame_thread_id.hits,(unsigned long long)frame_thread_id.fills,(unsigned long long)frame_thread_id.outside_hits);
 frame_thread_id.hits=frame_thread_id.fills=frame_thread_id.outside_hits=0;
 static const char *cull_kinds[]={"grid","segment","read_lock","read_unlock"};
 for(unsigned i=0;i<4;++i) {
  l_info("RIO_CULL_LOOKUP kind=%s calls=%llu samples=%u sampled_us=%llu worst_sample_us=%llu",
   cull_kinds[i],(unsigned long long)cull_lookup[i].calls,cull_lookup[i].samples,
   (unsigned long long)cull_lookup[i].us,(unsigned long long)cull_lookup[i].worst);
  cull_lookup[i].calls=cull_lookup[i].us=cull_lookup[i].worst=0;cull_lookup[i].samples=0;
 }
 for(unsigned i=0;i<registration_profile.used;++i) {
  RioRegistrationPass *p=&registration_profile.passes[i];
  l_info("RIO_REGISTER_PASS manager=0x%x root=0x%x camera=0x%x culler=0x%x calls=%u elapsed_us=%llu",
   (unsigned)p->manager,(unsigned)p->root,(unsigned)p->camera,(unsigned)p->culler,p->calls,(unsigned long long)p->us);
 }
 if(registration_profile.dropped)l_info("RIO_REGISTER_PASS dropped=%u",registration_profile.dropped);
 registration_profile=(RioRegistrationProfile){0};
 static uint64_t previous_wall,previous_cpu;
 SceKernelThreadInfo thread={.size=sizeof(thread)};
 uint64_t wall=sceKernelGetProcessTimeWide();
 if(!sceKernelGetThreadInfo(sceKernelGetThreadId(),&thread)) {
  if(previous_wall)l_info("RIO_PHASE_CPU wall_us=%llu run_clocks=%llu",
   (unsigned long long)(wall-previous_wall),(unsigned long long)(thread.runClocks-previous_cpu));
  previous_wall=wall;previous_cpu=thread.runClocks;
 }
 for(unsigned i=0;i<RIO_PHASE_COUNT;++i) {
  RioPhaseTotals *t=&phase_window[i];
  l_info("RIO_PHASE_WINDOW phase=%s calls=%u inclusive_us=%llu self_us=%llu worst_us=%llu",
   probes[PHASE_BASE+i].label,t->calls,(unsigned long long)t->inclusive,
   (unsigned long long)t->self,(unsigned long long)t->worst);
  *t=(RioPhaseTotals){0};
 }
#endif
}
void gangstar_native_reuse_frame(unsigned frame) {
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    reuse_frame = frame;
#else
    (void)frame;
#endif
}
void gangstar_native_reuse_report(void) {
#if defined(GANGSTAR_NATIVE_STARTUP_PROFILE) && defined(GANGSTAR_AUDIO_DIRECT_PATHS)
    l_info("RIO_AUDIO_DIRECT_PATH opened=%u missing=%u fallback=%u",
        atomic_exchange(&audio_path_opened,0),atomic_exchange(&audio_path_missing,0),atomic_exchange(&audio_path_fallback,0));
#endif

#ifdef GANGSTAR_DIRECT_ASSET_PATHS
    unsigned entries_built=0, folders_built=0;
    for (unsigned i=0;i<4;++i) {
        entries_built+=path_indexes[i].index.entry_builds; folders_built+=path_indexes[i].index.folder_builds;
        path_indexes[i].index.entry_builds=path_indexes[i].index.folder_builds=0;
    }
    l_info("RIO_DIRECT_PATH frame=%u opened=%u packed_missing=%u fallback=%u entry_builds=%u folder_builds=%u elapsed_us=%llu worst_us=%llu",
        reuse_frame,path_hits,path_missing,path_fallback,entries_built,folders_built,
        (unsigned long long)path_us,(unsigned long long)path_worst);
    path_hits=path_missing=path_fallback=0; path_us=path_worst=0;
#endif
#if defined(GANGSTAR_NATIVE_STARTUP_PROFILE) && defined(GANGSTAR_TEXTURE_REF_CACHE)
    pin_report();
#endif
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    // Copy under lock, log after releasing it so background cleanup can continue.
    __typeof__(gpu_release_events) released;
    unsigned released_count, released_calls, released_empty, released_dropped;
    pthread_mutex_lock(&gpu_release_lock);
    released_count = gpu_release_count; released_calls = gpu_release_calls;
    released_empty = gpu_release_empty; released_dropped = gpu_release_dropped;
    memcpy(released, gpu_release_events, released_count * sizeof(released[0]));
    gpu_release_count = gpu_release_calls = gpu_release_empty = gpu_release_dropped = 0;
    pthread_mutex_unlock(&gpu_release_lock);
    for (unsigned i = 0; i < released_count; ++i)
        l_info("RIO_GPU_RELEASE at_us=%llu frame=%u thread=%d texture=%p gl_name=%u caller=%p flags38=%u mask=%u elapsed_us=%llu",
            (unsigned long long)released[i].at_us, released[i].frame, released[i].thread,
            (void *)released[i].object, released[i].gl_name, (void *)released[i].caller,
            released[i].flags, released[i].mask, (unsigned long long)released[i].elapsed_us);
    l_info("RIO_GPU_RELEASE_COVERAGE calls=%u empty=%u dropped=%u", released_calls, released_empty, released_dropped);
#endif

#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    for (unsigned i = 0; i < eviction_count; ++i)
        l_info("RIO_EVICT frame=%u op=%s texture=%p caller=%p last_load_frame=%u load_serial=%u mask=%u ready=%u bound=%u removed=%u elapsed_us=%llu name=%s",
            eviction_events[i].frame, eviction_events[i].op, (void *)eviction_events[i].object,
            (void *)eviction_events[i].caller, eviction_events[i].last_load_frame, eviction_events[i].load_serial,
            eviction_events[i].mask, eviction_events[i].ready, eviction_events[i].bound, eviction_events[i].removed,
            (unsigned long long)eviction_events[i].us, eviction_events[i].name);
    l_info("RIO_EVICT_COVERAGE unbind=%u unload=%u remove=%u destroy=%u unknown_unbind=%u unknown_unload=%u unknown_remove=%u unknown_destroy=%u filtered=%u dropped=%u identity_collisions=%u",
        eviction_calls[0], eviction_calls[1], eviction_calls[2], eviction_calls[3],
        eviction_unknown[0], eviction_unknown[1], eviction_unknown[2], eviction_unknown[3],
        eviction_filtered, eviction_dropped, texture_identities.collisions);
    eviction_count = eviction_dropped = eviction_filtered = 0;
    memset(eviction_calls, 0, sizeof(eviction_calls)); memset(eviction_unknown, 0, sizeof(eviction_unknown));
#endif

#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    l_info("RIO_GPU_MEMORY vram_free=%u vram_total=%u ram_free=%u physical_free=%u",
        (unsigned)vglMemFree(VGL_MEM_VRAM), (unsigned)vglMemTotal(VGL_MEM_VRAM),
        (unsigned)vglMemFree(VGL_MEM_RAM), (unsigned)vglMemFree(VGL_MEM_SLOW));
#endif
#if defined(GANGSTAR_NATIVE_STARTUP_PROFILE) && defined(GANGSTAR_RESIDENT_TEXTURES)
    l_info("RIO_RESIDENT passes=%u kept=%u released=%u charged_textures=%u charged_bytes=%u free_vram=%u",
        resident_passes, resident_kept, resident_released, resident_policy.count, resident_policy.bytes,
        (unsigned)vglMemFree(VGL_MEM_VRAM));
    resident_passes = resident_kept = resident_released = 0;
#endif
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    for (unsigned i = 0; i < reuse_count; ++i)
        l_info("RIO_REUSE frame=%u occurrence=%u file_bytes=%u texture=%p elapsed_us=%llu texture_calls=%u texture_us=%llu cache_read_us=%llu cache_read_bytes=%llu name=%s",
            reuse_events[i].frame, reuse_events[i].occurrence, reuse_events[i].size, (void *)reuse_events[i].texture,
            (unsigned long long)reuse_events[i].us, reuse_events[i].calls,
            (unsigned long long)reuse_events[i].upload_us, (unsigned long long)reuse_events[i].read_us,
            (unsigned long long)reuse_events[i].read_bytes, reuse_events[i].name);
    if (reuse_dropped) l_info("RIO_REUSE dropped=%u", reuse_dropped);
    reuse_count = reuse_dropped = 0;
#endif
}
GangstarNativePerf gangstar_native_stats(void) {
    GangstarNativePerf result={.top="none"};
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    result.lookup=probes[13].us; result.dds=probes[21].us;
    result.animation=probes[22].us; result.resources=probes[24].us;
    result.world=probes[25].us; result.graphics=probes[26].us;
    result.streaming=probes[18].us; result.file_load=probes[19].us;
    for (unsigned i=0;i<sizeof(probes)/sizeof(*probes);++i)
        if (probes[i].us>result.top_us) {
            result.top=probes[i].label; result.top_us=probes[i].us; result.top_calls=probes[i].calls;
        }
#endif
    return result;
}
void gangstar_native_startup_frame(void) {
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    sequential_id_frame_begin();
    asset_count = asset_dropped = 0;
    for (unsigned i = 0; i < sizeof(probes) / sizeof(*probes); ++i) {
        probes[i].us = 0; probes[i].calls = 0;
    }
#ifdef GANGSTAR_ARCHIVE_ROUTE_CACHE
    route_hits = route_misses = route_learned = route_failed = route_packed_misses = route_skipped = 0;
    route_max_readers = route_max_folders = 0;

#endif
#ifdef GANGSTAR_PAK_NAME_LOOKUP
    atomic_store(&pak_fast, 0); atomic_store(&pak_fallback, 0);
#endif
#endif
}
void gangstar_native_startup_report(unsigned frame) {
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    audio_probe_report();
#endif
    gangstar_texture_lod_report(frame);
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    for (unsigned i = 0; i < asset_count; ++i)
        l_info("RIO_LOAD frame=%u elapsed_us=%llu bytes=%u op=%s name=%s", frame,
            (unsigned long long)asset_loads[i].us, asset_loads[i].bytes, asset_loads[i].op, asset_loads[i].name);
    if (asset_dropped) l_info("RIO_LOAD frame=%u omitted=%u", frame, asset_dropped);
    for (unsigned i = 0; i < sizeof(probes) / sizeof(*probes); ++i)
        if (probes[i].calls) {
            l_info("RIO_NATIVE frame=%u op=%s calls=%u elapsed_us=%llu", frame,
                probes[i].label, probes[i].calls, (unsigned long long)probes[i].us);
        }
#ifdef GANGSTAR_ARCHIVE_ROUTE_CACHE
    l_info("RIO_ROUTE_DIRECT entry_hits=%u generation_checks=%u full_folder_checks=%u generation=%u",
        route_entry_hits,atomic_exchange(&route_stamp_fast,0),atomic_exchange(&route_stamp_full,0),atomic_load(&route_generation));
    route_entry_hits=0;
    l_info("RIO_ROUTE frame=%u hits=%u misses=%u learned=%u failed=%u packed_miss_hits=%u skipped=%u max_readers=%u max_folders=%u", frame,
        route_hits, route_misses, route_learned, route_failed, route_packed_misses, route_skipped, route_max_readers, route_max_folders);
#endif

#ifdef GANGSTAR_PAK_NAME_LOOKUP
    l_info("RIO_PAK_LOOKUP frame=%u fast=%u fallback=%u", frame,
        atomic_load(&pak_fast), atomic_load(&pak_fallback));
#endif
#endif
}

// CGlfReadFile::clone returns an object even if its OBB stream failed to open.
// Native getPos/getSize unconditionally dereference that stream. Match seek's
// failure behavior, with one bounded attempt to reopen the owned reader.
static void (*native_stream_open)(void *);
static void *stream_backend(void *file) { return *(void **)((char *)file+12); }
static void reopen_stream(void *file) { native_stream_open(file); }
static int stream_position(void *stream) {
    uintptr_t *vtable=*(uintptr_t **)stream;
    return ((int (*)(void *))vtable[0x3c/4])(stream);
}
static int stream_size(void *stream) {
    uintptr_t *vtable=*(uintptr_t **)stream;
    return ((int (*)(void *))vtable[0x30/4])(stream);
}
static void stream_reopen_report(void *file,int ready) {
    static atomic_uint reports;
    if(atomic_fetch_add(&reports,1)<16)
        l_warn("RIO_STREAM_REOPEN ready=%d errno=%d path=%.192s",ready,errno,*(char **)((char *)file+16));
}
static int guarded_stream_position(void *file) {
    const RioStreamQuery ops={stream_backend,reopen_stream,stream_position,stream_reopen_report};
    return rio_stream_query(file,&ops);
}
static int guarded_stream_size(void *file) {
    const RioStreamQuery ops={stream_backend,reopen_stream,stream_size,stream_reopen_report};
    return rio_stream_query(file,&ops);
}
static void install_stream_guard(void) {
    uintptr_t position=so_symbol(&so_mod,"_ZNK6glitch2io12CGlfReadFile6getPosEv");
    uintptr_t size=so_symbol(&so_mod,"_ZNK6glitch2io12CGlfReadFile7getSizeEv");
    native_stream_open=(void *)so_symbol(&so_mod,"_ZN6glitch2io12CGlfReadFile8openFileEv");
    static const uint32_t prologue[]={0xe92d4010,0xe590300c};
    if(!position || !size || !native_stream_open ||
        memcmp((void *)position,prologue,sizeof(prologue)) ||
        memcmp((void *)size,prologue,sizeof(prologue))) {
        l_error("Gangstar stream guard: unsupported reader methods");return;
    }
    hook_addr(position,(uintptr_t)guarded_stream_position);
    hook_addr(size,(uintptr_t)guarded_stream_size);
    l_info("Gangstar stream guard: bounded reopen and failed-query handling installed");
}
#if defined(GANGSTAR_NATIVE_STARTUP_PROFILE) && defined(GANGSTAR_ARCHIVE_ROUTE_CACHE)
static struct RouteMutation {
    const char *symbol; uint32_t expected[2]; NativeCall original;
} route_mutations[]={
    {"_ZN17CCustomFileSystem19addCustomPakArchiveEPKcbb",{0xe92d4ff0,0xe1a04000},NULL},
    {"_ZN17CCustomFileSystem20addFolderFileArchiveEPKcbb",{0xe92d45f0,0xe24dd01c},NULL},
    {"_ZN17CCustomFileSystemC1ERN3glf2FsE",{0xe92d4070,0xe59f503c},NULL},
    {"_ZN17CCustomFileSystemC2ERN3glf2FsE",{0xe92d4070,0xe59f503c},NULL},
    {"_ZN17CCustomFileSystemD1Ev",{0xe59f3098,0xe59f2098},NULL}
};
static uintptr_t route_mutation(unsigned n,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {
    atomic_fetch_add(&route_generation,1);
    uintptr_t result=route_mutations[n].original(a,b,c,d);
    atomic_fetch_add(&route_generation,1);
    return result;
}
#define ROUTE_MUTATION(N) static uintptr_t route_mutation_##N(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d) {return route_mutation(N,a,b,c,d);}
ROUTE_MUTATION(0) ROUTE_MUTATION(1) ROUTE_MUTATION(2) ROUTE_MUTATION(3) ROUTE_MUTATION(4)
static void install_route_generations(void) {
    NativeCall wrappers[]={route_mutation_0,route_mutation_1,route_mutation_2,route_mutation_3,route_mutation_4};
    unsigned installed=0;
    for(unsigned i=0;i<5;++i) {
        struct RouteMutation *p=&route_mutations[i];
        uintptr_t address=so_symbol(&so_mod,p->symbol);
        uint32_t code[6],literal[2]={0};
        if(!address || address<so_mod.text_base || address+8>so_mod.text_base+so_mod.text_size ||
           memcmp((void *)address,p->expected,8))continue;
        for(unsigned n=0;n<2;++n)if((p->expected[n]&0xffff0000)==0xe59f0000)
            memcpy(&literal[n],(void *)(address+n*4+8+(p->expected[n]&4095)),4);
        if(!rio_startup_trampoline(code,address,p->expected,literal))continue;
        uintptr_t target=(so_mod.patch_head+3)&~(uintptr_t)3;
        if(target<so_mod.patch_base || target>so_mod.patch_base+so_mod.patch_size ||
           sizeof(code)>so_mod.patch_base+so_mod.patch_size-target)break;
        so_mod.patch_head=target+sizeof(code);
        memcpy((void *)target,code,sizeof(code));kuKernelFlushCaches((void *)target,sizeof(code));
        p->original=(NativeCall)target;hook_addr(address,(uintptr_t)wrappers[i]);
        kuKernelFlushCaches((void *)address,8);++installed;
    }
    route_generation_ready=installed==5;
    l_info("RIO_ROUTE_GENERATION hooks=%u ready=%d",installed,route_generation_ready);
}
#endif
// Native swapWeapon assumes a current weapon even during mission equipment
// transitions. Its sibling getCurrentWeapon explicitly permits no weapon.
static uintptr_t (*weapon_current)(uintptr_t,uintptr_t);
static uintptr_t (*weapon_in_list)(uintptr_t,int,uintptr_t);
static void (*weapon_swap_original)(uintptr_t,uintptr_t,uintptr_t);
static void guarded_weapon_swap(uintptr_t manager,uintptr_t character,uintptr_t info) {
    uintptr_t current=0;
    if(info) {
        int index;memcpy(&index,(void *)(info+0x124),sizeof(index));
        if(index>=0 && index<44) {
            current=*(unsigned char *)(info+0x128) ? weapon_in_list(manager,index,info) : weapon_current(manager,info);
        }
    }
    if(!current) {
        static unsigned reports;
        if(reports++<16)l_warn("RIO_WEAPON_SWAP skipped_empty_current info=0x%x",(unsigned)info);
        return;
    }
    weapon_swap_original(manager,character,info);
}
static void install_weapon_guard(void) {
    uintptr_t address=so_symbol(&so_mod,"_ZN13WeaponManager10swapWeaponER9CharacterR11WeaponInfos");
    uintptr_t current=so_symbol(&so_mod,"_ZN13WeaponManager16getCurrentWeaponER11WeaponInfos");
    uintptr_t list=so_symbol(&so_mod,"_ZN13WeaponManager15getWeaponInListEiR11WeaponInfos");
    const uint32_t expected[]={0xe5d23128,0xe92d4070};
    const uint32_t current_expected[]={0xe2512000,0x1a000002};
    uint32_t code[6],literal[2]={0};
    if(!address || !current || !list || memcmp((void *)address,expected,sizeof(expected)) ||
       memcmp((void *)current,current_expected,sizeof(current_expected)) ||
       !rio_startup_trampoline(code,address,expected,literal)) {
        l_error("Gangstar weapon guard: unsupported methods");return;
    }
    uintptr_t target=(so_mod.patch_head+3)&~(uintptr_t)3;
    if(target<so_mod.patch_base || target>so_mod.patch_base+so_mod.patch_size ||
       sizeof(code)>so_mod.patch_base+so_mod.patch_size-target) {
        l_error("Gangstar weapon guard: patch arena full");return;
    }
    so_mod.patch_head=target+sizeof(code);
    memcpy((void *)target,code,sizeof(code));kuKernelFlushCaches((void *)target,sizeof(code));
    weapon_current=(void *)current;weapon_in_list=(void *)list;weapon_swap_original=(void *)target;
    hook_addr(address,(uintptr_t)guarded_weapon_swap);kuKernelFlushCaches((void *)address,8);
    l_info("Gangstar weapon guard: empty current weapon handled");
}
void gangstar_native_startup_install(void) {
#ifdef GANGSTAR_SELECTIVE_VERTEX_COPY
    gangstar_vertex_scratch_install();
#endif
    install_stream_guard();
    install_weapon_guard();
#if defined(GANGSTAR_NATIVE_STARTUP_PROFILE) && defined(GANGSTAR_ARCHIVE_ROUTE_CACHE)
    install_route_generations();
#endif
    gangstar_audio_poll_init(so_mod.load_addr);
#if defined(GANGSTAR_NATIVE_STARTUP_PROFILE) && defined(GANGSTAR_AUDIO_HALF_CHUNKS)
    chunk_fill=so_symbol(&so_mod,"_ZN3vox22DecoderCursorInterface9DecodeRefERPvi");
    chunk_submit=so_symbol(&so_mod,"_ZThn8_N3vox29DriverCallbackSourceInterface10UploadDataEPvi");
    chunk_cursor_vtable=so_symbol(&so_mod,"_ZTVN3vox18DecoderMSWavCursorE");
    chunk_ima_vtable=so_symbol(&so_mod,"_ZTVN3vox26VoxMSWavSubDecoderIMAADPCME");
    if(chunk_cursor_vtable)chunk_cursor_vtable+=8;
    if(chunk_ima_vtable)chunk_ima_vtable+=8;
    l_info("RIO_AUDIO_CHUNK targets_ready=%d",chunk_fill && chunk_submit && chunk_cursor_vtable && chunk_ima_vtable);
#endif
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
    unsigned installed = 0;
#ifdef GANGSTAR_TEXTURE_PREWARM
#ifdef GANGSTAR_WORLD_SOURCE_PREWARM
    world_source_fs_vtable=so_symbol(&so_mod,"_ZTV17CCustomFileSystem");
    if (world_source_fs_vtable) world_source_fs_vtable+=8;
    world_source_file_release=(void *)so_symbol(&so_mod,"_ZN6glitch21intrusive_ptr_releaseEPKNS_17IReferenceCountedE");
#endif
    texture_prewarm_manager = (void *)so_symbol(&so_mod, "_Z17GetTextureManagerv");
    texture_prewarm_get = (NativeCall)so_symbol(&so_mod, "_ZN6glitch5video15CTextureManager10getTextureEPKcS3_");
#endif

#ifdef GANGSTAR_TEXTURE_REF_CACHE
    unsigned pin_cleanup_hooks = 0;
#endif
#ifdef GANGSTAR_TEXTURE_REF_CACHE
    texture_pin_add_ref = (void *)so_symbol(&so_mod, "_ZN6glitch21intrusive_ptr_add_refEPKNS_21IReferenceCountedBaseE");
    texture_pin_release_ref = (void *)so_symbol(&so_mod, "_ZN5boost13intrusive_ptrIN6glitch5video8ITextureEED1Ev");
    l_info("Gangstar texture ref cache: sixteen selected 512 textures, 3 MiB direct payload limit; ready=%d",
        texture_pin_add_ref != NULL && texture_pin_release_ref != NULL);
#endif
#ifdef GANGSTAR_RESIDENT_TEXTURES
    resident_properties = (void *)so_symbol(&so_mod, "_ZNK6glitch4core6detail15SIDedCollectionIN5boost13intrusive_ptrINS_5video8ITextureEEEtLb0ENS5_6detail14texturemanager18STexturePropertiesENS1_15sidedcollection12SValueTraitsEE13getPropertiesEt");
    resident_unbind = (void *)so_symbol(&so_mod, "_ZN6glitch5video8ITexture6unbindEjj");
    l_info("Gangstar ready texture residency: automatic pass only, 8 MiB accounting limit, 8 MiB VRAM reserve; ready=%d",
        resident_properties != NULL && resident_unbind != NULL);
#endif
#if defined(GANGSTAR_ASSET_PREFETCH) || defined(GANGSTAR_SMALL_LIGHTMAPS)
    direct_file_vtable = so_symbol(&so_mod, "_ZTVN6glitch2io12CGlfReadFileE");
    if (direct_file_vtable) direct_file_vtable += 8;
#ifdef GANGSTAR_SMALL_LIGHTMAPS
    make_memory_file = (void *)so_symbol(&so_mod, "_ZN6glitch2io20createMemoryReadFileEPvlPKcb");
    l_info("Gangstar world sources: DDS ETC0 -> %u, native-owned reduced buffers; ready=%d",
        GANGSTAR_TEXTURE_LOD_EDGE, direct_file_vtable != 0 && make_memory_file != NULL);
#endif
#endif
#ifdef GANGSTAR_ARCHIVE_ROUTE_CACHE
    pak_vtable = so_symbol(&so_mod, "_ZTV16CCustomPakReader");
    if (pak_vtable) pak_vtable += 8;
#endif
    for (unsigned i = 0; i < sizeof(probes) / sizeof(*probes); ++i) {
        if(!native_probe_needed(i))continue;
        struct Probe *p = &probes[i];
        uintptr_t address = so_symbol(&so_mod, p->symbol);
        if (!address || (address & 3) || address < so_mod.text_base ||
            address + 8 > so_mod.text_base + so_mod.text_size ||
            memcmp((void *)address, p->expected, sizeof(p->expected))) {
            l_warn("Native startup probe skipped: %s signature mismatch", p->label); continue;
        }
        uint32_t literal[2] = {0}; int valid = 1;
        for (unsigned n = 0; n < 2; ++n) {
            uint32_t word = p->expected[n];
            if ((word & 0xffff0000) == 0xe59f0000) {
                uintptr_t source = address + n * 4 + 8 + (word & 0xfff);
                if (source < so_mod.text_base || source + 4 > so_mod.text_base + so_mod.text_size) { valid = 0; break; }
                memcpy(&literal[n], (void *)source, 4);
            }
        }
        uint32_t code[6];
        if (!valid || !rio_startup_trampoline(code, address, p->expected, literal)) continue;
        uintptr_t target = (so_mod.patch_head + 3) & ~(uintptr_t)3;
        if (target < so_mod.patch_base || target > so_mod.patch_base + so_mod.patch_size ||
            sizeof(code) > so_mod.patch_base + so_mod.patch_size - target) {
            l_warn("Native startup probes: patch arena full"); break;
        }
        so_mod.patch_head = target + sizeof(code);
        memcpy((void *)target, code, sizeof(code));
        kuKernelFlushCaches((void *)target, sizeof(code));
        p->original = (NativeCall)target;
        hook_addr(address, (uintptr_t)wrappers[i]);
        kuKernelFlushCaches((void *)address, 8);
#ifdef GANGSTAR_TEXTURE_REF_CACHE
        if (i == 33) pin_cleanup_hooks |= 1;
        if (i == 34) pin_cleanup_hooks |= 2;
#endif
        ++installed;
    }
#ifdef GANGSTAR_TEXTURE_REF_CACHE
    texture_pin_hooks_ready = pin_cleanup_hooks == 3 && texture_pin_add_ref && texture_pin_release_ref;
    l_info("Gangstar texture ref cache cleanup guards: active=%d", texture_pin_hooks_ready);
#endif
    l_info("Native startup probes installed: %u", installed);
#endif
}
