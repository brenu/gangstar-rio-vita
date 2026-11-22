"""Pin the native targets behind GANGSTAR_DIRECT_DEVICE_MODEL and GANGSTAR_VITA_PERF_PROFILE."""
from pathlib import Path
import struct

e = Path('../work/libgl2jni.so').read_bytes()
word = lambda address: struct.unpack_from('<I', e, address)[0]
patch = Path('source/patch.c').read_text()

# AndroidGetDeviceModel prologue, as checked before hooking.
assert (word(0x546f28), word(0x546f2c)) == (0xe59f30ac, 0xe3a02000)
assert '0xe59f30ac, 0xe3a02000' in patch and '_Z21AndroidGetDeviceModelv' in patch
def bl_target(address):
    offset = word(address) & 0xffffff
    offset -= (offset & 0x800000) << 1
    assert word(address) >> 24 == 0xeb
    return address + 8 + offset * 4
# drawPrimitives<CProgrammableGLDriver>: every draw path ends in
# AndroidGetDeviceModel -> strstr(model, "shield") -> glFlush (PLT stubs).
assert bl_target(0x885c78) == 0x546f28
assert bl_target(0x885c84) == 0x371290 and bl_target(0x885c90) == 0x371044
assert e[0xd77be0:0xd77be7] == b'shield\0'
# One shared model string for the Java callback and the native answer.
assert 'value = RIO_PHONE_MODEL;' in Path('source/gangstar_java.c').read_text()
assert '#define RIO_PHONE_MODEL "PS Vita"' in Path('source/gangstar_device_model.h').read_text()

# Structs::PerfProfile::Read stores the overridden fields at these offsets...
for address, offset in ((0x3a6aec, 0x5c), (0x3a6af8, 0x60), (0x3a6de0, 0x8c), (0x3a6dec, 0x90),
                        (0x3a6df8, 0x94), (0x3a6e04, 0x98), (0x3a6eb4, 0xb8), (0x3a6ec0, 0xbc)):
    assert word(address) == 0xe5860000 | offset, hex(address)
# ...and NPCRoundRobinManager::UpdateProperties reads the NPC budget from them.
assert word(0x4ab7c8) == 0xe59200bc and word(0x4ab7d4) == 0xedd27a2e
profile = Path('source/gangstar_vita_profile.h').read_text()
for key, offset in (('farPlaneHighResDist', 0x5c), ('farPlaneLowResDist', 0x60), ('maxSpawn', 0x8c),
                    ('maxSpawnCivilians', 0x90), ('maxSpawnParkedVehicles', 0x94), ('maxSpawnVehicles', 0x98),
                    ('npcFullAnimationDistance', 0xb8), ('npcAnimationsPerFrame', 0xbc)):
    line = next(l for l in profile.splitlines() if l.strip().startswith('{"%s",' % key))
    assert ' 0x%x,' % offset in line, key
# The overrides run inside the verified customProfileForSpecialDevices hook.
assert word(0x3b51d4) == 0xe59f3030 and word(0x3b51d8) == 0xe79f3003
a = patch.index('static void gangstar_customize_profile(void)')
assert patch.index('gangstar_vita_profile_apply(profile);', a) < patch.index('static void gangstar_patch_hud_profile', a)
# Live readback: UpdateFarPlaneDistance stores the converted distances right
# after gPhonePerf (GOT slots 0x1548/0xf8c -> 0xe9cde8/0xe9cdec); the NPC
# distance float follows gNumberOfNPCPerFrame (slot 0x2a88 -> 0x101fb00).
got = 0xe854ac
assert word(got + 0x19f4) == 0xe9cd8c and word(got + 0x1548) == 0xe9cd8c + 0x5c and word(got + 0xf8c) == 0xe9cd8c + 0x60
assert word(got + 0x37a0) == 0x101fafc and word(got + 0x2a88) == 0x101fafc + 4 and word(got + 0xf9c) == 0x101fafc - 1
assert word(got + 0x3ca8) == 0x101fafa
print('Device model and Vita profile: native call sites, profile offsets and NPC budget readers and live readback addresses verified')
