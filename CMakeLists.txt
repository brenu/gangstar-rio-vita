cmake_minimum_required(VERSION 3.14)

if (NOT DEFINED ENV{VITASDK} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Please define VITASDK env var to point to your SDK path or set CMAKE_TOOLCHAIN_FILE.")
endif ()

set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Toolchain file")

project(so_loader C CXX)

include("${VITASDK}/share/vita.cmake" REQUIRED)

# Vita application configuration
set(VITA_APP_NAME "Gangstar Rio - WIP")
set(VITA_TITLEID "G4HM00001")
set(VITA_VPKNAME "gangstar_rio_wip")
set(VITA_VERSION "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

# Optional: used for "send" and "dump" targets
set(PSVITAIP "$ENV{PSVITAIP}" CACHE STRING "Optional Vita IP; deployment uses tools/vita_test.py")
set(PARSECOREPATH "$ENV{PARSECOREPATH}" CACHE STRING "vita-parse-core main.py")

# Game-specific definitions
set(DATA_PATH "ux0:/data/gangstarrio/")
set(SO_PATH "${DATA_PATH}libgl2jni.so")
add_definitions(-DDATA_PATH="${DATA_PATH}" -DSO_PATH="${SO_PATH}")

# Port type

option(USE_SCELIBC_IO "Use SceLibcBridge for IO and some other std functions" OFF)
if (USE_SCELIBC_IO)
  add_definitions(-DUSE_SCELIBC_IO)
endif()

set(SHADER_FORMAT "GLSL") # Preferred shader format (one of 'GLSL', 'CG', 'GXP')
if (${SHADER_FORMAT} STREQUAL "GLSL")
  add_definitions(-DUSE_GLSL_SHADERS)
elseif (${SHADER_FORMAT} STREQUAL "CG")
  add_definitions(-DUSE_CG_SHADERS)
elseif (${SHADER_FORMAT} STREQUAL "GXP")
  add_definitions(-DUSE_GXP_SHADERS)
else()
  message(FATAL_ERROR "SHADER_FORMAT option can only be one of: 'GLSL', 'CG', 'GXP'")
endif()

set(valid_dump_formats GLSL CG)
option(DUMP_COMPILED_SHADERS "When using GLSL or CG shader format, cache compiled shaders on disk" OFF)
if (DUMP_COMPILED_SHADERS)
  if (SHADER_FORMAT IN_LIST valid_dump_formats)
    add_definitions(-DDUMP_COMPILED_SHADERS)
  else()
    message(WARNING "DUMP_COMPILED_SHADERS has no effect for the chosen shader format ${SHADER_FORMAT}")
  endif()
endif()

# Unique debug definition for loader's logging to not interfere with anything
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG_SOLOADER)
  # Uncomment this for verbose FalsoJNI logging:
  #add_definitions(-DFALSOJNI_DEBUGLEVEL=0)
  # Uncomment this for OpenGL logging:
  #add_definitions(-DDEBUG_OPENGL)
  # Uncomment this for so_util logging:
  #add_definitions(-DSO_UTIL_VERBOSE=1)
  # Space-separated flags passed to vitaGL
  set(VITAGL_MAKE_FLAGS "SOFTFP_ABI=1 TEST=1 LOG_ERRORS=1")
else()
  # Space-separated flags passed to vitaGL
  set(VITAGL_MAKE_FLAGS "SOFTFP_ABI=1 NO_DEBUG=1")
endif()

# Reduce diagnostic overhead without disabling vitaGL argument validation.
option(GANGSTAR_PERFORMANCE_PROFILE "Quiet repeated diagnostics and optimize host bridge" OFF)
option(GANGSTAR_VITAGL_PROFILING "Report VitaGL draw and GPU-queue timing counters" OFF)
option(GANGSTAR_IO_PROFILE "Log slow guest file opens and reads" OFF)
option(GANGSTAR_HEAP_TRACE "Sample host allocations for short memory diagnostics" OFF)
option(GANGSTAR_GRAPHICS_DIAGNOSTICS "Enable expensive shader dumps and HUD/vertex tracing" OFF)
option(GANGSTAR_SHARED_ARCHIVE_CACHE "Cache read-only main/patch stdio and descriptor blocks across opens" ON)
option(GANGSTAR_DISTRICT_PRELOAD "Experimental background district source selection" OFF)
if(GANGSTAR_DISTRICT_PRELOAD)
  add_definitions(-DGANGSTAR_DISTRICT_PRELOAD)
endif()
option(GANGSTAR_WORLD_SOURCE_PREWARM "Preload all inventoried 512 world source mip chains during loading" OFF)
option(GANGSTAR_WORLD_TEXTURE_LOD "Use offline-inventoried static world texture mip candidates" OFF)

option(GANGSTAR_ALPHA_SUBIMAGE_OVERWRITE "Avoid preserving overwritten single-level alpha image bytes" OFF)
option(GANGSTAR_ETC1_RESERVE_MIPS "Reserve supported ETC1 mip chains on first additional mip upload" OFF)
set(GANGSTAR_ARCHIVE_CACHE_MIB "16" CACHE STRING "Shared archive data cache size in MiB (16, 32 or 64)")
if(NOT GANGSTAR_ARCHIVE_CACHE_MIB MATCHES "^(16|32|64)$")
  message(FATAL_ERROR "GANGSTAR_ARCHIVE_CACHE_MIB must be 16, 32 or 64")
endif()
add_definitions(-DRIO_CACHE_MIB=${GANGSTAR_ARCHIVE_CACHE_MIB})
set(GANGSTAR_ARCHIVE_BLOCK_KIB "128" CACHE STRING "Shared archive cache block size in KiB")
if (NOT GANGSTAR_ARCHIVE_BLOCK_KIB MATCHES "^(32|64|128)$")
  message(FATAL_ERROR "GANGSTAR_ARCHIVE_BLOCK_KIB must be 32, 64 or 128")
endif()
add_definitions(-DRIO_CACHE_BLOCK_KIB=${GANGSTAR_ARCHIVE_BLOCK_KIB})
option(GANGSTAR_ARCHIVE_VIRTUAL_CURSOR "Defer physical read-cursor advances on private archive streams" OFF)
if(GANGSTAR_ARCHIVE_VIRTUAL_CURSOR)
  add_definitions(-DGANGSTAR_ARCHIVE_VIRTUAL_CURSOR)
endif()
option(GANGSTAR_ARCHIVE_READ_POSITION_CACHE "Avoid redundant position queries before private archive reads" OFF)
if(GANGSTAR_ARCHIVE_READ_POSITION_CACHE)
  add_definitions(-DGANGSTAR_ARCHIVE_READ_POSITION_CACHE)
endif()
option(GANGSTAR_ARCHIVE_POSITION_CACHE "Cache known position queries for unexposed archive FILE handles" OFF)
if (GANGSTAR_ARCHIVE_POSITION_CACHE)
  add_definitions(-DGANGSTAR_ARCHIVE_POSITION_CACHE)
endif()
option(GANGSTAR_AUDIO_HALF_CHUNKS "Test half-size chunks on verified four-buffer native IMA streams" OFF)
if (GANGSTAR_AUDIO_HALF_CHUNKS)
  add_definitions(-DGANGSTAR_AUDIO_HALF_CHUNKS)
endif()
option(GANGSTAR_NATIVE_IMA_FAST "Use verified native IMA block decoding with original engine stream state" OFF)
if (GANGSTAR_NATIVE_IMA_FAST)
  add_definitions(-DGANGSTAR_NATIVE_IMA_FAST)
endif()
option(GANGSTAR_NATIVE_ADPCM_VERIFY "Compare optimized native ADPCM blocks against original PCM without changing playback" OFF)
if (GANGSTAR_NATIVE_IMA_FAST AND NOT GANGSTAR_NATIVE_ADPCM_VERIFY)
  message(FATAL_ERROR "GANGSTAR_NATIVE_IMA_FAST requires GANGSTAR_NATIVE_ADPCM_VERIFY hooks")
endif()
if (GANGSTAR_NATIVE_ADPCM_VERIFY)
  add_definitions(-DGANGSTAR_NATIVE_ADPCM_VERIFY)
endif()
option(GANGSTAR_AUDIO_POLL_CAP "Cap verified emitter-worker polling sleeps at 8ms" OFF)
if(GANGSTAR_AUDIO_POLL_CAP)
  add_definitions(-DGANGSTAR_AUDIO_POLL_CAP)
endif()
option(GANGSTAR_AUDIO_SCOPED_PRIORITY "Test priority 160 during native emitter updates on worker 191" OFF)
if (GANGSTAR_AUDIO_SCOPED_PRIORITY)
  add_definitions(-DGANGSTAR_AUDIO_SCOPED_PRIORITY)
endif()
option(GANGSTAR_LOOSE_PATH_INDEX "Skip asset probes beneath pack roots proven absent at startup" OFF)
if (GANGSTAR_LOOSE_PATH_INDEX)
  add_definitions(-DGANGSTAR_LOOSE_PATH_INDEX)
endif()
option(GANGSTAR_LOOSE_IO_BUFFERING "Buffer ordinary game file reads and batched save writes" OFF)
if (GANGSTAR_LOOSE_IO_BUFFERING)
  add_definitions(-DGANGSTAR_LOOSE_IO_BUFFERING)
endif()
option(GANGSTAR_QUEUED_MUTEX "Queue contended Android mutex waiters instead of timer polling" OFF)
if (GANGSTAR_QUEUED_MUTEX)
  add_definitions(-DGANGSTAR_QUEUED_MUTEX)
endif()
option(GANGSTAR_UPLOAD_STAGES "Measure native ETC upload allocation and swizzle stages" OFF)
option(GANGSTAR_ASYNC_LOG "Write diagnostic records on a bounded background logger" OFF)
if (GANGSTAR_ASYNC_LOG)
  add_definitions(-DGANGSTAR_ASYNC_LOG)
endif()
option(GANGSTAR_AUDIO_DIRECT_PATHS "Use separate direct archive indexes for emitter-update opens" OFF)
if(GANGSTAR_AUDIO_DIRECT_PATHS)
  add_definitions(-DGANGSTAR_AUDIO_DIRECT_PATHS)
endif()
option(GANGSTAR_DIRECT_ASSET_PATHS "Resolve original OBB paths through an in-memory metadata index" OFF)
if(GANGSTAR_AUDIO_DIRECT_PATHS AND NOT GANGSTAR_DIRECT_ASSET_PATHS)
  message(FATAL_ERROR "Audio direct paths require GANGSTAR_DIRECT_ASSET_PATHS")
endif()
option(GANGSTAR_TEXTURE_PREWARM "Queue selected world textures during streaming initialization" OFF)
option(GANGSTAR_TEXTURE_REF_CACHE "Retain bounded native refs for selected 512 world textures" OFF)
option(GANGSTAR_RESIDENT_TEXTURES "Exempt bounded ready textures from automatic GPU unload" OFF)
option(GANGSTAR_HIDE_TOUCH_BUTTONS "Hide touch control drawings without disabling HUD/input" OFF)
if (GANGSTAR_HIDE_TOUCH_BUTTONS)
  add_definitions(-DGANGSTAR_HIDE_TOUCH_BUTTONS)
endif()
set(GANGSTAR_TEXTURE_LOD_EDGE "1024" CACHE STRING "Reduced world texture edge (512 or 1024)")
if (NOT GANGSTAR_TEXTURE_LOD_EDGE STREQUAL "512" AND NOT GANGSTAR_TEXTURE_LOD_EDGE STREQUAL "1024")
  message(FATAL_ERROR "GANGSTAR_TEXTURE_LOD_EDGE must be 512 or 1024")
endif()
add_definitions(-DGANGSTAR_TEXTURE_LOD_EDGE=${GANGSTAR_TEXTURE_LOD_EDGE})
option(GANGSTAR_SMALL_LIGHTMAPS "Use retained lower DDS mips for verified large world lightmaps" OFF)
option(GANGSTAR_ASSET_PREFETCH "Prefetch bounded texture ranges at native pak open" OFF)
option(GANGSTAR_ARCHIVE_PREFETCH "Read ahead one archive block after sequential access" OFF)
option(GANGSTAR_NATIVE_ARCHIVE_IO "Use validated native Vita handles for archive cache fills" OFF)
option(GANGSTAR_NATIVE_STARTUP_PROFILE "Time verified native script-loading methods" OFF)
option(GANGSTAR_DETAILED_RENDER_PROFILE "Enable detailed native renderer and GL timing hooks" OFF)
if(GANGSTAR_DETAILED_RENDER_PROFILE)
  add_definitions(-DGANGSTAR_DETAILED_RENDER_PROFILE)
endif()
option(GANGSTAR_ARCHIVE_ROUTE_CACHE "Cache successful immutable archive dispatch routes" OFF)
if(GANGSTAR_AUDIO_DIRECT_PATHS AND NOT GANGSTAR_ARCHIVE_ROUTE_CACHE)
  message(FATAL_ERROR "Audio direct paths require GANGSTAR_ARCHIVE_ROUTE_CACHE")
endif()
option(GANGSTAR_PAK_NAME_LOOKUP "Avoid heap allocation in verified Rio pak name lookup" OFF)
if (GANGSTAR_WORLD_SOURCE_PREWARM)
  if (NOT GANGSTAR_WORLD_TEXTURE_LOD OR NOT GANGSTAR_TEXTURE_PREWARM OR NOT GANGSTAR_TEXTURE_REF_CACHE)
    message(FATAL_ERROR "World source prewarm requires world512 policy and native texture prewarm/ref cache")
  endif()
  add_definitions(-DGANGSTAR_WORLD_SOURCE_PREWARM)
endif()
if (GANGSTAR_WORLD_TEXTURE_LOD)
  if (NOT GANGSTAR_SMALL_LIGHTMAPS OR NOT GANGSTAR_TEXTURE_LOD_EDGE STREQUAL "512")
    message(FATAL_ERROR "Whole-world texture LOD requires source reduction and 512 edge")
  endif()
  add_definitions(-DGANGSTAR_WORLD_TEXTURE_LOD)
endif()
if (GANGSTAR_DIRECT_ASSET_PATHS)
  if (NOT GANGSTAR_ARCHIVE_ROUTE_CACHE OR NOT GANGSTAR_NATIVE_STARTUP_PROFILE)
    message(FATAL_ERROR "Direct asset paths require archive routing and native profiling")
  endif()
  add_definitions(-DGANGSTAR_DIRECT_ASSET_PATHS)
endif()
if (GANGSTAR_TEXTURE_PREWARM)
  if (NOT GANGSTAR_TEXTURE_REF_CACHE)
    message(FATAL_ERROR "Texture prewarm requires native texture ref cache")
  endif()
  add_definitions(-DGANGSTAR_TEXTURE_PREWARM)
endif()
if (GANGSTAR_TEXTURE_REF_CACHE)
  if (NOT GANGSTAR_NATIVE_STARTUP_PROFILE OR NOT GANGSTAR_SMALL_LIGHTMAPS OR NOT GANGSTAR_TEXTURE_LOD_EDGE STREQUAL "512")
    message(FATAL_ERROR "Texture ref cache requires native profiling and 512 texture edge")
  endif()
  add_definitions(-DGANGSTAR_TEXTURE_REF_CACHE)
endif()
if (GANGSTAR_RESIDENT_TEXTURES)
  if (NOT GANGSTAR_NATIVE_STARTUP_PROFILE)
    message(FATAL_ERROR "Resident texture experiment requires native profiling")
  endif()
  add_definitions(-DGANGSTAR_RESIDENT_TEXTURES)
endif()
if (GANGSTAR_SMALL_LIGHTMAPS)
  if (NOT GANGSTAR_NATIVE_STARTUP_PROFILE)
    message(FATAL_ERROR "Small lightmaps require GANGSTAR_NATIVE_STARTUP_PROFILE=ON")
  endif()
  add_definitions(-DGANGSTAR_SMALL_LIGHTMAPS)
endif()
if (GANGSTAR_ASSET_PREFETCH)
  if (NOT GANGSTAR_NATIVE_STARTUP_PROFILE)
    message(FATAL_ERROR "Asset prefetch requires GANGSTAR_NATIVE_STARTUP_PROFILE=ON")
  endif()
  add_definitions(-DGANGSTAR_ASSET_PREFETCH)
endif()
if (GANGSTAR_PAK_NAME_LOOKUP)
  if (NOT GANGSTAR_NATIVE_STARTUP_PROFILE)
    message(FATAL_ERROR "Pak lookup experiment requires GANGSTAR_NATIVE_STARTUP_PROFILE=ON")
  endif()
  add_definitions(-DGANGSTAR_PAK_NAME_LOOKUP)
endif()
if (GANGSTAR_ARCHIVE_ROUTE_CACHE)
  if (NOT GANGSTAR_NATIVE_STARTUP_PROFILE)
    message(FATAL_ERROR "Route-cache experiment requires GANGSTAR_NATIVE_STARTUP_PROFILE=ON")
  endif()
  add_definitions(-DGANGSTAR_ARCHIVE_ROUTE_CACHE)
endif()
if (GANGSTAR_NATIVE_STARTUP_PROFILE)
  add_definitions(-DGANGSTAR_NATIVE_STARTUP_PROFILE)
endif()
option(GANGSTAR_SHADER_WARMUP "Record successful GLSL pairs and warm them before engine startup" ON)
if (NOT GANGSTAR_SHADER_WARMUP)
  add_definitions(-DGANGSTAR_DISABLE_SHADER_WARMUP)
endif()
if (GANGSTAR_NATIVE_ARCHIVE_IO)
  add_definitions(-DGANGSTAR_NATIVE_ARCHIVE_IO)
endif()
if (NOT GANGSTAR_ARCHIVE_PREFETCH)
  add_definitions(-DGANGSTAR_DISABLE_PREFETCH)
endif()
if (NOT GANGSTAR_SHARED_ARCHIVE_CACHE)
  add_definitions(-DGANGSTAR_DISABLE_ARCHIVE_CACHE)
endif()
if (GANGSTAR_GRAPHICS_DIAGNOSTICS)
  add_definitions(-DGANGSTAR_GRAPHICS_DIAGNOSTICS)
endif()
option(GANGSTAR_VITAGL_WVP_ON_GPU "Move fixed-function world-view-projection work to the GPU" OFF)
option(GANGSTAR_VITAGL_NO_TILE_CLIPPER "Trade GPU work for lower CPU scissor/tile clipping overhead" OFF)
option(GANGSTAR_VITAGL_SKIP_ERROR_HANDLING "Skip VitaGL validation/error checks for performance" OFF)
option(GANGSTAR_VITAGL_SAFE_DRAW_SPEEDHACK "Allow direct GPU reads of large client vertex buffers (experimental DRAW_SPEEDHACK=2)" OFF)
option(GANGSTAR_VITAGL_NO_DMAC "Use CPU copies instead of sceDmacMemcpy in VitaGL" OFF)
set(GANGSTAR_HOST_OPT_LEVEL "2" CACHE STRING "Host bridge optimization level for performance profile (2 or 3)")
if (GANGSTAR_PERFORMANCE_PROFILE)
  if (NOT GANGSTAR_HOST_OPT_LEVEL MATCHES "^[23]$")
    message(FATAL_ERROR "GANGSTAR_HOST_OPT_LEVEL must be 2 or 3")
  endif()
  add_definitions(-DGANGSTAR_QUIET_DIAGNOSTICS)
  set(VITAGL_MAKE_FLAGS "SOFTFP_ABI=1")
endif()
if (GANGSTAR_VITAGL_WVP_ON_GPU)
  set(VITAGL_MAKE_FLAGS "${VITAGL_MAKE_FLAGS} HAVE_WVP_ON_GPU=1")
endif()
if (GANGSTAR_VITAGL_NO_TILE_CLIPPER)
  set(VITAGL_MAKE_FLAGS "${VITAGL_MAKE_FLAGS} NO_TILE_CLIPPER=1")
endif()
if (GANGSTAR_VITAGL_SKIP_ERROR_HANDLING)
  set(VITAGL_MAKE_FLAGS "${VITAGL_MAKE_FLAGS} NO_DEBUG=1")
endif()
if (GANGSTAR_VITAGL_SAFE_DRAW_SPEEDHACK)
  set(VITAGL_MAKE_FLAGS "${VITAGL_MAKE_FLAGS} DRAW_SPEEDHACK=2")
endif()
if (GANGSTAR_VITAGL_NO_DMAC)
  set(VITAGL_MAKE_FLAGS "${VITAGL_MAKE_FLAGS} NO_DMAC=1")
endif()
if (GANGSTAR_IO_PROFILE)
  add_definitions(-DGANGSTAR_IO_PROFILE)
endif()
if (GANGSTAR_VITAGL_PROFILING)
  set(VITAGL_MAKE_FLAGS "${VITAGL_MAKE_FLAGS} HAVE_PROFILING=1")
endif()

# makes sincos, sincosf, etc. visible
add_definitions(-D_GNU_SOURCE -D__POSIX_VISIBLE=999999)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -mfloat-abi=softfp -std=gnu11 -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-q -mfloat-abi=softfp -std=gnu++20 -Wno-write-strings -Wno-psabi")

if (GANGSTAR_PERFORMANCE_PROFILE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O${GANGSTAR_HOST_OPT_LEVEL} -g3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O${GANGSTAR_HOST_OPT_LEVEL} -g3")
    # The Vita Debug configuration appends -O0 after these global flags.
    # Keep its DEBUG definitions/assertions, but put the selected level last.
    string(APPEND CMAKE_C_FLAGS_DEBUG " -O${GANGSTAR_HOST_OPT_LEVEL}")
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -O${GANGSTAR_HOST_OPT_LEVEL}")
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O1 -g3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1 -g3")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g0 -ffast-math")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g0 -ffast-math")
endif()

set(SOURCES
    source/main.c
    source/gangstar_platform.c
    source/gangstar_platform_data.c
    source/gangstar_library_policy.c
    source/gangstar_dynlink.c
    source/dynlib.c
    source/gangstar_jni_tables.c
    source/gangstar_perf.c
    source/gangstar_java.c
    source/gangstar_jni_strings.c
    source/gangstar_trophies.c
    source/gangstar_controls.c
    source/gangstar_audio_output.c
    source/gangstar_archive_cache.c
    source/gangstar_audio_poll.c
    source/gangstar_audio_probe.c
    source/gangstar_file_names.c
    source/gangstar_loose_index.c
    source/gangstar_archive_io.c
    source/gangstar_district_preload.c
    source/gangstar_render_sample.c
    source/gangstar_texture_lod.c
    source/gangstar_asset_perf.c
    source/gangstar_wait_perf.c
    source/gangstar_native_startup.c
    source/gangstar_shader_warmup.c
    source/gangstar_capture.c
    source/patch.c
    source/reimpl/bits/_ctype.c
    source/reimpl/errno.c
    source/reimpl/io.c
    source/reimpl/log.c
    source/reimpl/mem.c
    source/gangstar_mutex.c
    source/reimpl/pthr.c
    source/reimpl/sys.c
    source/reimpl/time64.c
    source/utils/dialog.c
    source/utils/glutil.c
    source/utils/init.c
    source/utils/logger.c
    source/utils/settings.c
    source/utils/utils.c
    lib/falso_jni/FalsoJNI.c
    lib/falso_jni/FalsoJNI_ImplBridge.c
    lib/falso_jni/FalsoJNI_Logger.c
    lib/falso_jni/converter.c
    lib/sha1/sha1.c
    lib/fios/fios.c
    lib/so_util/so_util.c)

list(APPEND SOURCES
  source/reimpl/asset_manager.cpp
  source/reimpl/controls.c
  source/reimpl/controls.h)

add_library(rio_adpcm STATIC source/gangstar_adpcm.c source/gangstar_adpcm_stream.c)
target_compile_options(rio_adpcm PRIVATE -O3)
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
target_link_libraries(${CMAKE_PROJECT_NAME} rio_adpcm)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
  "-Wl,--wrap=sceAudioOutOpenPort,--wrap=sceAudioOutSetConfig,--wrap=sceAudioOutOutput")


if(GANGSTAR_HEAP_TRACE)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE source/gangstar_heap_wrap.c)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE GANGSTAR_HEAP_TRACE)
  target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,--wrap=malloc,--wrap=calloc,--wrap=memalign,--wrap=realloc,--wrap=free,--wrap=_malloc_r,--wrap=_calloc_r,--wrap=_memalign_r,--wrap=_realloc_r,--wrap=_free_r")
endif()

add_subdirectory(lib/libc_bridge)
add_dependencies(${CMAKE_PROJECT_NAME} SceLibcBridge)


# VitaGL built from lib/vitagl via its own Makefile.
# Made this way so that testing quick adjustments to VitaGL is possible,
# as well as changing build flags in a fast and efficient way.
# Gangstar mixes interleaved positions/normals with separate UV streams.
# The default packing heuristic only checks the first two client attributes.
# Strict draw compliance validates each stream instead of reusing stream zero.
if (GANGSTAR_ALPHA_SUBIMAGE_OVERWRITE)
  string(APPEND VITAGL_MAKE_FLAGS " ALPHA_SUBIMAGE_OVERWRITE=1")
endif()
if (GANGSTAR_ETC1_RESERVE_MIPS)
  string(APPEND VITAGL_MAKE_FLAGS " ETC1_RESERVE_MIPS=1")
endif()
if (GANGSTAR_UPLOAD_STAGES)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE GANGSTAR_UPLOAD_STAGES)
  string(APPEND VITAGL_MAKE_FLAGS " UPLOAD_STAGES=1")
endif()
option(GANGSTAR_VERTEX_TRACE "Enable on-demand single-frame vertex and texture metadata tracing" OFF)
if(GANGSTAR_VERTEX_TRACE)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE GANGSTAR_VERTEX_TRACE)
endif()
option(GANGSTAR_SELECTIVE_VERTEX_COPY "Snapshot native temporary vertex streams while retaining full draw speedhack elsewhere" OFF)
if(GANGSTAR_SELECTIVE_VERTEX_COPY)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE source/gangstar_vertex_scratch.c)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE GANGSTAR_SELECTIVE_VERTEX_COPY)
  if(NOT GANGSTAR_VITAGL_FULL_DRAW_SPEEDHACK)
    message(FATAL_ERROR "Selective snapshots require DRAW_SPEEDHACK=1")
  endif()
  string(APPEND VITAGL_MAKE_FLAGS " RIO_SELECTIVE_VERTEX_COPY=1")
endif()
option(GANGSTAR_VITAGL_STORE_DEPTH_STENCIL "Preserve depth and stencil across rendering scenes" OFF)
if(GANGSTAR_VITAGL_STORE_DEPTH_STENCIL)
  string(APPEND VITAGL_MAKE_FLAGS " STORE_DEPTH_STENCIL=1")
endif()
option(GANGSTAR_DIRECT_DRAW_SYNC "Diagnostic completion wait after client draw calls" OFF)
if(GANGSTAR_DIRECT_DRAW_SYNC)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE GANGSTAR_DIRECT_DRAW_SYNC)
endif()
option(GANGSTAR_VITAGL_FULL_DRAW_SPEEDHACK "Experiment with direct reads for all client vertex draws" OFF)
if(GANGSTAR_VITAGL_FULL_DRAW_SPEEDHACK)
  if(GANGSTAR_VITAGL_SAFE_DRAW_SPEEDHACK)
    message(FATAL_ERROR "Choose either full DRAW_SPEEDHACK=1 or large-buffer DRAW_SPEEDHACK=2")
  endif()
  string(APPEND VITAGL_MAKE_FLAGS " DRAW_SPEEDHACK=1")
endif()
option(GANGSTAR_VITAGL_CIRCULAR_POOL_SPEEDHACK "Experiment with one wrapping temporary GPU buffer" OFF)
if(GANGSTAR_VITAGL_CIRCULAR_POOL_SPEEDHACK)
  string(APPEND VITAGL_MAKE_FLAGS " CIRCULAR_POOL_SPEEDHACK=1")
endif()
option(GANGSTAR_VITAGL_INDICES_DRAW_SPEEDHACK "Experiment with direct client index reads" OFF)
if(GANGSTAR_VITAGL_INDICES_DRAW_SPEEDHACK)
  string(APPEND VITAGL_MAKE_FLAGS " INDICES_DRAW_SPEEDHACK=1")
endif()
option(GANGSTAR_VITAGL_BUFFERS_SPEEDHACK "Experiment with in-place VBO updates" OFF)
if(GANGSTAR_VITAGL_BUFFERS_SPEEDHACK)
  string(APPEND VITAGL_MAKE_FLAGS " BUFFERS_SPEEDHACK=1")
endif()
option(GANGSTAR_VITAGL_MATH_SPEEDHACK "Use VitaGL fast matrix transforms" OFF)
if(GANGSTAR_VITAGL_MATH_SPEEDHACK)
  string(APPEND VITAGL_MAKE_FLAGS " MATH_SPEEDHACK=1")
endif()
option(GANGSTAR_VERTEX_CPU_POOLS "Give temporary vertex buffers enough CPU-side pool capacity" OFF)
if(GANGSTAR_VERTEX_CPU_POOLS)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE GANGSTAR_VERTEX_CPU_POOLS)
endif()
option(GANGSTAR_VERTEX_RANGE_COPY "Trim unused client vertex prefixes in indexed triangle draws" OFF)
if(GANGSTAR_VERTEX_RANGE_COPY)
  string(APPEND VITAGL_MAKE_FLAGS " RIO_VERTEX_RANGE_COPY=1")
endif()
option(GANGSTAR_VERTEX_PATCH_CACHE "Reuse identical live vertex patch selections" OFF)
if(GANGSTAR_VERTEX_PATCH_CACHE)
  string(APPEND VITAGL_MAKE_FLAGS " RIO_VERTEX_PATCH_CACHE=1")
endif()
string(APPEND VITAGL_MAKE_FLAGS " SAFE_DRAW=1")
# This revision's GC worker advances the producer's global queue/index. Running
# it concurrently with draw uploads can overwrite retirement entries. Collect
# on the render thread, preserving the existing four-frame retirement delay.
string(APPEND VITAGL_MAKE_FLAGS " SINGLE_THREADED_GC=1")
string(APPEND VITAGL_MAKE_FLAGS " HAVE_SHADER_CACHE=1")
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  "GANGSTAR_VITAGL_BUILD_FLAGS=\"${VITAGL_MAKE_FLAGS}\"")


separate_arguments(VITAGL_MAKE_ARGS UNIX_COMMAND "${VITAGL_MAKE_FLAGS}")

set(VITAGL_DIR ${CMAKE_SOURCE_DIR}/lib/vitagl)
set(VITAGL_LIB ${VITAGL_DIR}/libvitaGL.a)

# Force a full rebuild of vitaGL whenever VITAGL_MAKE_FLAGS change
set(VITAGL_FLAGS_STAMP ${CMAKE_BINARY_DIR}/vitagl_flags.stamp)
set(vitagl_old_flags "")
if (EXISTS ${VITAGL_FLAGS_STAMP})
  file(READ ${VITAGL_FLAGS_STAMP} vitagl_old_flags)
endif()
if (NOT vitagl_old_flags STREQUAL VITAGL_MAKE_FLAGS)
  file(GLOB_RECURSE vitagl_stale_objs ${VITAGL_DIR}/source/*.o)
  file(REMOVE ${vitagl_stale_objs} ${VITAGL_LIB})
  file(WRITE ${VITAGL_FLAGS_STAMP} "${VITAGL_MAKE_FLAGS}")
endif()

include(ProcessorCount)
ProcessorCount(VITAGL_NPROC)
if (VITAGL_NPROC EQUAL 0)
  set(VITAGL_NPROC 4)
endif()

add_custom_target(vitaGL_build
                  # Nested sources include <vitaGL.h>; use the bundled header
                  # even when vitaGL has not previously been installed in SDK.
                  COMMAND make -C ${VITAGL_DIR} -j${VITAGL_NPROC}
                          "CPPFLAGS=-I${VITAGL_DIR}/source" ${VITAGL_MAKE_ARGS}
                  BYPRODUCTS ${VITAGL_LIB}
                  COMMENT "Building VitaGL"
                  VERBATIM)

add_library(vitaGL STATIC IMPORTED)
set_target_properties(vitaGL PROPERTIES
                      IMPORTED_LOCATION ${VITAGL_LIB}
                      INTERFACE_INCLUDE_DIRECTORIES ${VITAGL_DIR}/source)
add_dependencies(${CMAKE_PROJECT_NAME} vitaGL_build)

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM
                           PUBLIC ${CMAKE_SOURCE_DIR}/lib
                           )

target_include_directories(${CMAKE_PROJECT_NAME}
                           PUBLIC ${CMAKE_SOURCE_DIR}/source
                           )


target_link_libraries(${CMAKE_PROJECT_NAME}
                      -Wl,--whole-archive pthread -Wl,--no-whole-archive
                      OpenSLES
                      sndfile
                      mp3lame
                      mpg123
                      opus
                      vorbis
                      ogg
                      vorbisenc
                      vorbisfile
                      FLAC
                      stdc++
                      vitaGL
                      vitashark
                      SceShaccCgExt
                      taihen_stub
                      SceShaccCg_stub
                      z
                      m
                      ScePgf_stub
                      mathneon
                      kubridge_stub
                      ${CMAKE_BINARY_DIR}/lib/libc_bridge/stubs/libSceLibcBridge_stub.a
                      SceHid_stub
                      SceTouch_stub
                      SceMotion_stub
                      SceAppMgr_stub
                      SceAppUtil_stub
                      SceAudio_stub
                      SceAudioIn_stub
                      SceCommonDialog_stub
                      SceDisplay_stub
                      SceKernelDmacMgr_stub
                      SceFios2_stub
                      SceCtrl_stub
                      SceGxm_stub
                      ScePower_stub
                      SceTouch_stub
                      SceVshBridge_stub
                      )

vita_create_self(eboot.bin ${CMAKE_PROJECT_NAME} UNSAFE NOASLR)
vita_create_vpk(${VITA_VPKNAME}.vpk ${VITA_TITLEID} eboot.bin
                VERSION ${VITA_VERSION}
                NAME ${VITA_APP_NAME}
                FILE ${CMAKE_SOURCE_DIR}/extras/cpuinfo cpuinfo
                ${CMAKE_SOURCE_DIR}/extras/meminfo meminfo
                ${CMAKE_SOURCE_DIR}/extras/urandom urandom
                ${CMAKE_SOURCE_DIR}/extras/livearea/icon0.png sce_sys/icon0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/pic0.png sce_sys/pic0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/startup.png sce_sys/livearea/contents/startup.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/bg0.png sce_sys/livearea/contents/bg0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/template.xml sce_sys/livearea/contents/template.xml
                )

# Hardware deployment is explicit through tools/vita_test.py; see docs/INSTALLING.md.
