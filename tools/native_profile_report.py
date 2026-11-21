#!/usr/bin/env python3
"""Summarize RIO_PROF_FN lines from a port.log: engine functions by self time in slow frames."""
import collections, re, sys
from pathlib import Path
names = {}
names_file = Path(__file__).resolve().parents[1] / 'work/generated/native_profile_sites.txt'
if not names_file.exists():
    sys.exit('missing %s: build once with GANGSTAR_NATIVE_PROFILER=ON (or run tools/generate_native_profile_sites.py)' % names_file)
for line in names_file.read_text().splitlines():
    addr, name = line.split(' ', 1); names[int(addr, 16)] = name
self_us, incl_us, calls, frames = collections.Counter(), collections.Counter(), collections.Counter(), collections.Counter()
slow = total = 0
for line in open(sys.argv[1], errors='replace'):
    m = re.search(r'RIO_PROF frame=\d+ total_us=(\d+)', line)
    if m: slow += 1; total += int(m.group(1)); continue
    m = re.search(r'RIO_PROF_FN frame=\d+ fn=0x([0-9a-f]+) self_us=(\d+) incl_us=(\d+) calls=(\d+)', line)
    if m:
        fn = int(m.group(1), 16); self_us[fn] += int(m.group(2)); incl_us[fn] += int(m.group(3))
        calls[fn] += int(m.group(4)); frames[fn] += 1
print(f'{slow} slow frames, {total/1e6:.2f} s total')
print(f'{"self s":>7} {"incl s":>7} {"frames":>6} {"calls":>7}  function')
for fn, us in self_us.most_common(int(sys.argv[2]) if len(sys.argv) > 2 else 40):
    print(f'{us/1e6:7.2f} {incl_us[fn]/1e6:7.2f} {frames[fn]:6d} {calls[fn]:7d}  {names.get(fn, hex(fn))[:150]}')
