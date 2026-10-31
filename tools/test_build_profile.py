#!/usr/bin/env python3
"""Check that a fresh build selects the gameplay profile, including digit-named options."""
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
options = set(re.findall(r'-D(GANGSTAR_[A-Z0-9_]+)=', (root / 'build.sh').read_text()))
profile = (root / 'config/tested.sh').read_text()
defined = set(re.findall(r'\$\{(GANGSTAR_[A-Z0-9_]+):=', profile))
declared = set(re.findall(r'(?:option|set)\((GANGSTAR_[A-Z0-9_]+)', (root / 'CMakeLists.txt').read_text()))
assert options == declared, f'CMake/build mismatch: missing={declared-options}, unknown={options-declared}'
assert options == defined, f'Build/profile mismatch: missing={options-defined}, unused={defined-options}'
env = {k: v for k, v in os.environ.items() if not k.startswith('GANGSTAR_')}
script = 'source "$1"; shift; for key in "$@"; do printf "%s=%s\\n" "$key" "${!key}"; done'

def values(overrides):
    out = subprocess.check_output(['bash', '-c', script, 'profile-test',
        str(root / 'config/tested.sh'), *sorted(options)], env={**env, **overrides}, text=True)
    return dict(line.split('=', 1) for line in out.splitlines())

actual = values({})
required = {
    'GANGSTAR_ARCHIVE_CACHE_MIB': '32', 'GANGSTAR_ARCHIVE_BLOCK_KIB': '32',
    'GANGSTAR_ETC1_RESERVE_MIPS': 'ON', 'GANGSTAR_AUDIO_DIRECT_PATHS': 'ON',
    'GANGSTAR_ARCHIVE_VIRTUAL_CURSOR': 'ON', 'GANGSTAR_NATIVE_IMA_FAST': 'ON',
    'GANGSTAR_TEXTURE_LOD_EDGE': '512', 'GANGSTAR_AUDIO_POLL_CAP': 'OFF',
    'GANGSTAR_VITAGL_NO_TILE_CLIPPER': 'ON', 'GANGSTAR_HOST_OPT_LEVEL': '3',
}
required.update({key:'ON' for key in ['GANGSTAR_SELECTIVE_VERTEX_COPY', 'GANGSTAR_VERTEX_CPU_POOLS', 'GANGSTAR_VERTEX_RANGE_COPY', 'GANGSTAR_VITAGL_BUFFERS_SPEEDHACK', 'GANGSTAR_VITAGL_FULL_DRAW_SPEEDHACK', 'GANGSTAR_VITAGL_MATH_SPEEDHACK', 'GANGSTAR_VITAGL_SKIP_ERROR_HANDLING', 'GANGSTAR_VITAGL_STORE_DEPTH_STENCIL']})
for key, expected in required.items():
    assert actual[key] == expected, (key, actual[key], expected)
overrides = {'GANGSTAR_ETC1_RESERVE_MIPS': 'OFF', 'GANGSTAR_ARCHIVE_BLOCK_KIB': '64'}
assert all(values(overrides)[k] == v for k, v in overrides.items())
print(f'Build profile: all {len(options)} options supplied, preferred defaults and explicit overrides passed')

# The public preview must show the same values that the profile test reads.
preview = subprocess.check_output(['bash', str(root / 'build.sh'), '--show-config'], env=env, text=True)
selected = dict(line[2:].split('=', 1) for line in preview.splitlines())
assert all(selected[key] == value for key, value in actual.items())
assert selected['CMAKE_BUILD_TYPE'] == 'Debug' and selected['USE_SCELIBC_IO'] == 'OFF'

import importlib.util
import tempfile
spec = importlib.util.spec_from_file_location('verify_build_config', root / 'tools/verify_build_config.py')
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)
with tempfile.TemporaryDirectory() as folder:
    directory = Path(folder)
    cache = directory / 'CMakeCache.txt'
    cache.write_text('GANGSTAR_ARCHIVE_BLOCK_KIB:STRING=32\n')
    verifier.verify(directory, ['-DGANGSTAR_ARCHIVE_BLOCK_KIB=32'])
    assert (directory / 'build-config.json').exists()
    try:
        verifier.verify(directory, ['-DGANGSTAR_ARCHIVE_BLOCK_KIB=128'])
    except ValueError:
        pass
    else:
        raise AssertionError('Configuration drift was accepted')
print('Build preview matches the profile; configured-setting drift is rejected')
