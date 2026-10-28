#include <psp2/audioout.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
int __wrap_sceAudioOutSetConfig(int, SceSize, int, int);
static int seen[4], calls, result_mode;
int __real_sceAudioOutSetConfig(int port, SceSize len, int freq, int mode) {
    assert(port == 17);
    (void)len; (void)freq;
    seen[calls++] = mode;
    if (result_mode == 1) return 0;
    if (result_mode == 2) return -123;
    return mode == 255 ? -99 : 0;
}
int __real_sceAudioOutOpenPort(SceAudioOutPortType t, int n, int f, SceAudioOutMode m) {
    (void)t; (void)n; (void)f; (void)m; return 17;
}
int __real_sceAudioOutOutput(int port, const void *buffer) {
    (void)port; (void)buffer; return 0;
}
void _log_print(int type, const char *fmt, ...) { (void)type; (void)fmt; }
int main(void) {
    // Reproduce the legacy library's narrowed enum, then verify the actual
    // wrapper passes a full signed -1 to the kernel boundary on retry.
    SceAudioOutMode legacy = (SceAudioOutMode)-1;
    assert(sizeof(legacy) == 1 && (int)legacy == 255);
    assert(__wrap_sceAudioOutSetConfig(17, (SceSize)-1, -1, legacy) == 0);
    assert(calls == 2 && seen[0] == 255 && seen[1] == -1);
    calls = 0; result_mode = 1;
    assert(__wrap_sceAudioOutSetConfig(17, (SceSize)-1, -1, 255) == 0 && calls == 1);
    calls = 0; result_mode = 2;
    assert(__wrap_sceAudioOutSetConfig(17, 128, -1, 255) == -123 && calls == 1);
    calls = 0;
    assert(__wrap_sceAudioOutSetConfig(17, (SceSize)-1, 44100, 255) == -123 && calls == 1);
    calls = 0;
    assert(__wrap_sceAudioOutSetConfig(17, (SceSize)-1, -1, 1) == -123 && calls == 1);
    puts("Audio ABI: legacy byte enum repaired to signed -1; success and other requests preserved");
}
