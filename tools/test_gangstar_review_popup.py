"""The review popup gate: initial value, and the guarded branch before its only use."""
from pathlib import Path
import struct
p = next((x for x in (Path('../work/libgl2jni.so'), Path('work/libgl2jni.so')) if x.exists()), None)
if not p:
    print('Review popup: skipped (libgl2jni.so not available)')
    raise SystemExit
d = p.read_bytes()
phoff = struct.unpack_from('<I', d, 28)[0]
segments = [struct.unpack_from('<8I', d, phoff + 32 * i) for i in range(struct.unpack_from('<H', d, 44)[0])]
def word(v):
    for t, offset, address, _, size, *_ in segments:
        if t == 1 and address <= v <= address + size - 4:
            return struct.unpack_from('<I', d, offset + v - address)[0]
assert word(0xe8a338) & 0xff == 1
got = (word(0x3b8064) + 0x3b7aa0) & 0xffffffff
assert word(got + word(0x3b80d8)) == 0xe8a338
# ldrb r3,[r6]; cmp r3,#0; beq past the popup; ... bl showReviewPopup2
assert [word(a) for a in (0x3b7f84, 0x3b7f88, 0x3b7f8c)] == [0xe5d63000, 0xe3530000, 0x0affff3b]
assert word(0x3b7fa4) == 0xeb081b26
source = Path('source/patch.c').read_text()
assert '"is_show_review"' in source and '    gangstar_review_popup_guard();' in source
print('Review popup: native flag value, gate branch and popup call verified')
