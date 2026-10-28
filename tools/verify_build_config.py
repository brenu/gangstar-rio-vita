#!/usr/bin/env python3
"""Fail on configuration drift and save the actual settings beside the VPK."""
import json
from pathlib import Path
import sys


def verify(directory, arguments):
    expected = dict(arg[2:].split('=', 1) for arg in arguments if arg.startswith('-D'))
    actual = {}
    for line in (directory / 'CMakeCache.txt').read_text().splitlines():
        if line and not line.startswith(('#', '//')) and ':' in line and '=' in line:
            key, value = line.split('=', 1)
            actual[key.split(':', 1)[0]] = value
    differences = {key: {'requested': value, 'configured': actual.get(key)}
        for key, value in expected.items() if actual.get(key) != value}
    if differences:
        raise ValueError('CMake configuration differs from build.sh: ' + json.dumps(differences))
    stamp = directory / 'vitagl_flags.stamp'
    flags_file = directory / 'CMakeFiles/so_loader.dir/flags.make'
    target_flags = {}
    if flags_file.exists():
        for line in flags_file.read_text().splitlines():
            if line.startswith(('C_FLAGS =', 'CXX_FLAGS =', 'C_DEFINES =', 'CXX_DEFINES =')):
                key, value = line.split('=', 1)
                target_flags[key.strip()] = value.strip()
    report = {'configured_options': {key: actual[key] for key in sorted(expected)},
        'vitagl_flags': stamp.read_text() if stamp.exists() else None,
        'target_compiler_flags': target_flags,
        'note': 'Build settings, not proof of identical runtime memory allocation or hardware performance.'}
    (directory / 'build-config.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    try:
        report = verify(Path(sys.argv[1]), sys.argv[2:])
    except (ValueError, OSError) as error:
        raise SystemExit(str(error))
    print(f'Verified {len(report["configured_options"])} CMake options; saved work/build/build-config.json')
