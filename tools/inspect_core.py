#!/usr/bin/env python3
"""Identify a Vita ELF crash dump before attempting symbolication.

Accepts raw ELF or gzip-compressed .psp2dmp files, including .tmp names.
Sony note payloads are not Linux NT_PRSTATUS records. This inspector only
reports bounded note strings, thread registers and segment completeness,
not an unwound backtrace. Thread record offsets follow xyzz/vita-parse-core.
"""
import argparse
import zlib
import hashlib
import json
import re
import struct
from pathlib import Path


def inspect(path):
    raw = path.read_bytes()
    compression_complete = True
    if raw[:2] == b'\x1f\x8b':
        decoder = zlib.decompressobj(16 + zlib.MAX_WBITS)
        data = decoder.decompress(raw)
        compression_complete = decoder.eof
    else:
        data = raw
    if len(data) < 52 or data[:6] != b'\x7fELF\x01\x01':
        raise ValueError('Expected a 32-bit little-endian ELF core')
    if struct.unpack_from('<HH', data, 16) != (4, 40):
        raise ValueError('Expected an ARM ET_CORE file')
    phoff = struct.unpack_from('<I', data, 28)[0]
    entsize, count = struct.unpack_from('<HH', data, 42)
    if entsize != 32 or phoff + entsize * count > len(data):
        raise ValueError('Invalid or incomplete ELF program header table')
    notes, records, incomplete = {}, {}, []
    for i in range(count):
        kind, offset, _, _, size, _, _, _ = struct.unpack_from('<8I', data, phoff + i * entsize)
        if offset + size > len(data):
            incomplete.append(i)
        if kind != 4 or offset + 12 > len(data):
            continue
        namesize, descsize, _ = struct.unpack_from('<3I', data, offset)
        if namesize > size - 12 or offset + 12 + namesize > len(data):
            continue
        name = data[offset + 12:offset + 12 + namesize].split(b'\0')[0].decode('ascii', 'replace')
        start = offset + 12 + ((namesize + 3) & ~3)
        payload = data[start:min(start + descsize, offset + size, len(data))]
        records[name] = payload
        if name in ('PROCESS_INFO', 'MODULE_INFO', 'THREAD_INFO', 'APP_INFO'):
            notes[name] = [s.decode('ascii') for s in re.findall(rb'[ -~]{5,}', payload)]
    process = notes.get('PROCESS_INFO', [])
    def entries(name, minimum):
        payload = records.get(name, b'')
        if len(payload) < 8:
            return
        count = struct.unpack_from('<I', payload, 4)[0]
        offset = 8
        for _ in range(count):
            if offset + 4 > len(payload):
                raise ValueError(f'Incomplete {name} entry')
            size = struct.unpack_from('<I', payload, offset)[0]
            if size < minimum or offset + size > len(payload):
                raise ValueError(f'Invalid {name} record size')
            yield payload[offset:offset + size]
            offset += size
    registers, faults = {}, {}
    for record in entries('THREAD_REG_INFO', 72):
        tid = struct.unpack_from('<I', record, 4)[0]
        registers[tid] = {f'r{i}': hex(value) for i, value in
                          enumerate(struct.unpack_from('<16I', record, 8))}
        # Extended ARM status layout: TibixDev/vita-crashdump reg_info.
        if len(record) >= 0x178:
            faults[tid] = {name: hex(struct.unpack_from('<I', record, offset)[0])
                           for name, offset in [('cpsr', 0x48), ('ifsr', 0x168),
                                                ('ifar', 0x16c), ('dfsr', 0x170),
                                                ('dfar', 0x174)]}
    threads = []
    for record in entries('THREAD_INFO', 160):
        tid = struct.unpack_from('<I', record, 4)[0]
        threads.append({'tid': hex(tid),
                        'name': record[8:40].split(b'\0')[0].decode('ascii', 'replace'),
                        'stop_reason': hex(struct.unpack_from('<I', record, 0x74)[0]),
                        'pc': hex(struct.unpack_from('<I', record, 0x9c)[0]),
                        'registered_stack_base': hex(struct.unpack_from('<I', record, 0x38)[0]),
                        'registered_stack_size': hex(struct.unpack_from('<I', record, 0x3c)[0]),
                        'registers': registers.get(tid), 'fault_registers': faults.get(tid)})
    return {'source': str(path.resolve()), 'sha256': hashlib.sha256(raw).hexdigest(),
            'elf_bytes': len(data), 'compression_complete': compression_complete,
            'segments_extending_past_eof': incomplete,
            'process_strings': process,
            'executable_paths': [s for s in process if '/eboot.bin' in s],
            'module_strings': notes.get('MODULE_INFO', []),
            'thread_strings': notes.get('THREAD_INFO', []),
            'app_strings': notes.get('APP_INFO', []), 'threads': threads}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dump', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        text = json.dumps(inspect(args.dump), indent=2) + '\n'
    except (ValueError, OSError, EOFError, struct.error, zlib.error) as exc:
        parser.exit(1, f'Cannot inspect dump: {exc}\n')
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end='')
