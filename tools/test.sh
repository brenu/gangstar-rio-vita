#!/usr/bin/env bash
set -euo pipefail
GANGSTAR_TEST_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$GANGSTAR_TEST_ROOT"
python3 tools/test_build_profile.py
mkdir -p work/tests
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_buttons.c -o work/tests/gangstar_buttons
work/tests/gangstar_buttons
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_audio_poll.c -o work/tests/gangstar_audio_poll
work/tests/gangstar_audio_poll
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_audio_chunk.c -o work/tests/gangstar_audio_chunk
work/tests/gangstar_audio_chunk
cc -std=c11 -O3 -Wall -Wextra -Werror -I source source/gangstar_adpcm.c tools/test_gangstar_adpcm.c -o work/tests/gangstar_adpcm
cc -std=c11 -O3 -Wall -Wextra -Werror -I source source/gangstar_adpcm.c source/gangstar_adpcm_stream.c tools/test_gangstar_adpcm_stream.c -o work/tests/gangstar_adpcm_stream
work/tests/gangstar_adpcm_stream
work/tests/gangstar_adpcm
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_audio_sched.c -o work/tests/gangstar_audio_sched
work/tests/gangstar_audio_sched
cc -std=c11 -O2 -Wall -Wextra -Werror -I source -pthread source/gangstar_audio_probe.c tools/test_gangstar_audio_probe.c -o work/tests/gangstar_audio_probe
work/tests/gangstar_audio_probe
cc -std=c11 -O2 -Wall -Wextra -Werror -I source -pthread source/gangstar_file_names.c tools/test_gangstar_file_names.c -o work/tests/gangstar_file_names
work/tests/gangstar_file_names
cc -std=c11 -O2 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -I source source/gangstar_loose_index.c tools/test_gangstar_loose_index.c -o work/tests/gangstar_loose_index
work/tests/gangstar_loose_index
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_obb_path.c -o work/tests/gangstar_obb_path
work/tests/gangstar_obb_path
cc -std=c11 -O2 -Wall -Wextra -Werror -I source -pthread source/gangstar_mutex.c tools/test_gangstar_mutex.c -o work/tests/gangstar_mutex
work/tests/gangstar_mutex
cc -std=c11 -O2 -Wall -Wextra -Werror -I lib/vitagl/source/utils tools/test_gangstar_alpha_overwrite.c -o work/tests/gangstar_alpha_overwrite
work/tests/gangstar_alpha_overwrite
cc -std=c11 -O2 -Wall -Wextra -Werror -I lib/vitagl/source/utils tools/test_gangstar_etc_reserve.c -o work/tests/gangstar_etc_reserve
work/tests/gangstar_etc_reserve
cc -std=c11 -O2 -Wall -Wextra -Werror -I lib/vitagl/source/utils tools/test_gangstar_retire_queue.c -o work/tests/gangstar_retire_queue
work/tests/gangstar_retire_queue
cc -std=c11 -O2 -Wall -Wextra -Werror -I lib/vitagl/source/utils tools/test_gangstar_compressed_mips.c -o work/tests/gangstar_compressed_mips
work/tests/gangstar_compressed_mips
cc -std=c11 -O2 -D_POSIX_C_SOURCE=200809L -DDEBUG_SOLOADER -DGANGSTAR_ASYNC_LOG '-DDATA_PATH="test/"' -Wall -Wextra -Werror -I tools/logger_mocks -I source -pthread source/utils/logger.c tools/test_gangstar_logger.c -o work/tests/gangstar_logger
work/tests/gangstar_logger
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_path_index.c -o work/tests/gangstar_path_index
work/tests/gangstar_path_index
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_texture_pin.c -o work/tests/gangstar_texture_pin
work/tests/gangstar_texture_pin
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_texture_identity.c -o work/tests/gangstar_texture_identity
work/tests/gangstar_texture_identity
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_resident.c -o work/tests/gangstar_resident
work/tests/gangstar_resident
for edge in 1024 512; do
cc -std=c11 -O2 -D_XOPEN_SOURCE=700 -DGANGSTAR_SMALL_LIGHTMAPS -DGANGSTAR_TEXTURE_LOD_EDGE=$edge -Wl,--wrap=open -Wl,--wrap=fstat -Wall -Wextra -Werror -I source    -pthread source/gangstar_texture_lod.c tools/test_gangstar_texture_lod.c -o work/tests/gangstar_texture_lod
work/tests/gangstar_texture_lod
done
cc -std=c11 -O2 -D_XOPEN_SOURCE=700 -DGANGSTAR_SMALL_LIGHTMAPS -DGANGSTAR_WORLD_TEXTURE_LOD -DGANGSTAR_TEXTURE_LOD_EDGE=512 -Wl,--wrap=open -Wl,--wrap=fstat -Wall -Wextra -Werror -I source -pthread source/gangstar_texture_lod.c tools/test_gangstar_texture_lod.c -o work/tests/gangstar_world_texture_lod
work/tests/gangstar_world_texture_lod --original
work/tests/gangstar_world_texture_lod
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_pak_lookup.c -o work/tests/gangstar_pak_lookup
work/tests/gangstar_pak_lookup
cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_pedals.c -o work/tests/gangstar_pedals
work/tests/gangstar_pedals
cc -std=c11 -O2 -Wall -Wextra -Werror -I source    tools/test_gangstar_route_cache.c -o work/tests/gangstar_route_cache
work/tests/gangstar_route_cache
cc -std=c11 -O2 -Wall -Wextra -Werror -I source    tools/test_gangstar_startup_trampoline.c -o work/tests/gangstar_startup_trampoline
work/tests/gangstar_startup_trampoline
cc -std=c11 -O2 -DDEBUG_SOLOADER -Wall -Wextra -Werror -I tools/test_mocks -I source    source/gangstar_wait_perf.c tools/test_gangstar_wait_perf.c -o work/tests/gangstar_wait_perf
work/tests/gangstar_wait_perf
cc -std=c11 -O2 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -I source -pthread    source/gangstar_trophies.c tools/test_gangstar_trophies.c -o work/tests/gangstar_trophies
work/tests/gangstar_trophies
cc -std=c11 -O2 -DDEBUG_SOLOADER -DGANGSTAR_UPLOAD_STAGES -Wall -Wextra -Werror -I tools/test_mocks -I source    source/gangstar_asset_perf.c tools/test_gangstar_asset_perf.c -lz -o work/tests/gangstar_asset_perf
work/tests/gangstar_asset_perf
cc -std=c11 -O2 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -I tools/test_mocks -I source    source/gangstar_shader_warmup.c tools/test_gangstar_shader_warmup.c -o work/tests/gangstar_shader_warmup
work/tests/gangstar_shader_warmup
cc -std=c11 -O2 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -I source -pthread    source/gangstar_archive_cache.c tools/test_gangstar_archive_cache.c -o work/tests/gangstar_archive_cache
work/tests/gangstar_archive_cache
cc -std=c11 -O2 -D_GNU_SOURCE -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -I tools/test_mocks -I source -pthread    source/gangstar_archive_cache.c source/gangstar_archive_io.c tools/test_gangstar_archive_io.c -Wl,--wrap=lseek -o work/tests/gangstar_archive_io
for block_kib in 32 64; do
  cc -std=c11 -O2 -D_XOPEN_SOURCE=700 -DRIO_CACHE_BLOCK_KIB="$block_kib" -Wall -Wextra -Werror -I source -pthread      source/gangstar_archive_cache.c tools/test_gangstar_archive_cache.c -o work/tests/gangstar_archive_cache_small
  work/tests/gangstar_archive_cache_small
  cc -std=c11 -O2 -D_GNU_SOURCE -D_XOPEN_SOURCE=700 -DRIO_CACHE_BLOCK_KIB="$block_kib" -DGANGSTAR_ARCHIVE_POSITION_CACHE -DGANGSTAR_ARCHIVE_READ_POSITION_CACHE -DGANGSTAR_ARCHIVE_VIRTUAL_CURSOR -Wall -Wextra -Werror -I tools/test_mocks -I source -pthread      source/gangstar_archive_cache.c source/gangstar_archive_io.c tools/test_gangstar_archive_io.c -Wl,--wrap=lseek -o work/tests/gangstar_archive_io_small
  work/tests/gangstar_archive_io_small
done
work/tests/gangstar_archive_io
cc -std=c11 -Wall -Wextra -Werror -I tools/test_mocks -I source    source/gangstar_audio_output.c tools/test_gangstar_audio_output.c -o work/tests/gangstar_audio_output
work/tests/gangstar_audio_output
cc -std=c11 -Wall -Wextra -Werror -I source    tools/test_gangstar_analog.c -lm -o work/tests/gangstar_analog
work/tests/gangstar_analog
cc -std=c11 -Wall -Wextra -Werror -I source    tools/test_memory_policy.c -o work/tests/memory_policy
work/tests/memory_policy
cc -std=c11 -Wall -Wextra -Werror -I source    source/gangstar_platform_data.c tools/test_platform_data.c -o work/tests/platform_data
work/tests/platform_data
cc -std=c11 -Wall -Wextra -Werror -I source    source/gangstar_library_policy.c tools/test_library_policy.c -o work/tests/library_policy
work/tests/library_policy
cc -std=c11 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -I source    tools/test_android_stat.c -o work/tests/android_stat
work/tests/android_stat
cc -std=c11 -O2 -Wall -Wextra -Werror -I source    tools/test_object_set.c -o work/tests/object_set
work/tests/object_set
cc -std=c11 -Wall -Wextra -Werror -I source    tools/test_perf_policy.c -o work/tests/perf_policy
work/tests/perf_policy

# Exercise the independent virtual-cursor flag with tell/read shortcuts off.
cc -std=c11 -O2 -D_GNU_SOURCE -D_XOPEN_SOURCE=700 -DGANGSTAR_ARCHIVE_VIRTUAL_CURSOR -Wall -Wextra -Werror -I tools/test_mocks -I source -pthread source/gangstar_archive_cache.c source/gangstar_archive_io.c tools/test_gangstar_archive_io.c -Wl,--wrap=lseek -o work/tests/gangstar_archive_virtual_cursor
work/tests/gangstar_archive_virtual_cursor

# Larger compiled cache plus smaller runtime capacity fallback.
cc -std=c11 -O2 -D_XOPEN_SOURCE=700 -DRIO_CACHE_MIB=64 -DRIO_CACHE_BLOCK_KIB=32 -Wall -Wextra -Werror -I source -pthread source/gangstar_archive_cache.c tools/test_gangstar_archive_cache.c -o work/tests/gangstar_archive_cache_64m
work/tests/gangstar_archive_cache_64m
cc -std=c11 -O2 -D_GNU_SOURCE -D_XOPEN_SOURCE=700 -DRIO_CACHE_MIB=64 -DRIO_CACHE_BLOCK_KIB=32 -DGANGSTAR_ARCHIVE_POSITION_CACHE -DGANGSTAR_ARCHIVE_READ_POSITION_CACHE -DGANGSTAR_ARCHIVE_VIRTUAL_CURSOR -Wall -Wextra -Werror -I tools/test_mocks -I source -pthread source/gangstar_archive_cache.c source/gangstar_archive_io.c tools/test_gangstar_archive_io.c -Wl,--wrap=lseek -o work/tests/gangstar_archive_io_64m
work/tests/gangstar_archive_io_64m

cc -std=c11 -O2 -D_GNU_SOURCE -D_XOPEN_SOURCE=700 -DRIO_CACHE_MIB=64 -DRIO_CACHE_BLOCK_KIB=32 -DRIO_TEST_FAIL_LARGE -DGANGSTAR_ARCHIVE_VIRTUAL_CURSOR -Wall -Wextra -Werror -I tools/test_mocks -I source -pthread source/gangstar_archive_cache.c source/gangstar_archive_io.c tools/test_gangstar_archive_io.c -Wl,--wrap=lseek -o work/tests/gangstar_archive_io_fallback
work/tests/gangstar_archive_io_fallback

cc -std=c11 -O2 -Wall -Wextra -Werror -I tools/test_mocks source/gangstar_heap_wrap.c tools/test_gangstar_heap_wrap.c -o work/tests/gangstar_heap_wrap
work/tests/gangstar_heap_wrap

python3 tools/test_heap_ledger.py

cc -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_jni_classes.c -o work/tests/gangstar_jni_classes
work/tests/gangstar_jni_classes

cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_stream_guard.c -o work/tests/gangstar_stream_guard
work/tests/gangstar_stream_guard

cc -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -I tools/test_mocks -I source -pthread source/gangstar_archive_cache.c source/gangstar_archive_io.c tools/test_gangstar_shared_archive.c -Wl,--wrap=open -o work/tests/gangstar_shared_archive
work/tests/gangstar_shared_archive

cc -std=c11 -O2 -Wall -Wextra -Werror -I source -I lib -pthread source/gangstar_jni_strings.c tools/test_gangstar_jni_strings.c -Wl,--wrap=calloc,--wrap=free -o work/tests/gangstar_jni_strings
work/tests/gangstar_jni_strings

cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_source_cache.c -Wl,--wrap=malloc,--wrap=free -o work/tests/gangstar_source_cache
work/tests/gangstar_source_cache

python3 tools/test_gangstar_weapon_guard.py

python3 tools/test_gangstar_route_generation.py

cc -std=c11 -O2 -Wall -Wextra -Werror -pthread -DRIO_CACHE_BLOCK_KIB=32 -I source tools/test_gangstar_archive_batch.c source/gangstar_archive_cache.c -o work/tests/archive_batch
work/tests/archive_batch

cc -std=c11 -D_GNU_SOURCE -DGANGSTAR_WORLD_TEXTURE_LOD -DGANGSTAR_TEXTURE_LOD_EDGE=512 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_district.c -lm -o work/tests/district
work/tests/district
python3 tools/test_gangstar_district_plan.py

cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_phase_timing.c -o work/tests/phase_timing
work/tests/phase_timing

cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_render_sample.c -o work/tests/render_sample
work/tests/render_sample

python3 tools/test_gangstar_render_hooks.py

cc -std=c11 -O2 -Wall -Wextra -Werror -I lib/vitagl/source/utils tools/test_gangstar_vertex_patch.c -o work/tests/vertex_patch
work/tests/vertex_patch

python3 tools/test_gangstar_vertex_patch_calls.py
cc -std=c11 -O2 -Wall -Wextra -Werror -I lib/vitagl/source/utils tools/test_gangstar_vertex_range.c -o work/tests/vertex_range
work/tests/vertex_range
python3 tools/test_gangstar_vertex_range_draw.py
python3 tools/test_gangstar_vertex_range_streams.py
cc -std=c11 -O2 -Wall -Wextra -Werror -I lib/vitagl/source/utils tools/test_gangstar_index_max.c -o work/tests/index_max
work/tests/index_max
python3 tools/test_gangstar_active_index_scan.py
python3 tools/test_gangstar_cull_thread_id.py
python3 tools/test_gangstar_profile_disabled.py
python3 tools/test_gangstar_clone_name.py
python3 tools/test_gangstar_selective_vertex.py
python3 tools/test_gangstar_vertex_scratch.py

python3 tools/test_gangstar_vertex_pool_alignment.py

cc -std=c11 -O2 -Wall -Wextra -Werror -I source tools/test_gangstar_registration_profile.c -o work/tests/registration_profile
work/tests/registration_profile
python3 tools/test_gangstar_trace_idle.py
python3 tools/test_gangstar_trace_draw_timing.py

cc -std=c11 -O2 -Wall -Wextra -Werror -Isource tools/test_gangstar_vertex_watch.c -o work/tests/vertex_watch
work/tests/vertex_watch

python3 tools/test_gangstar_partial_lightmap.py

python3 tools/test_gangstar_touch_update.py

python3 tools/test_gangstar_recovery.py
