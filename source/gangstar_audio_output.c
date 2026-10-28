/* Observe the OpenSL hardware output and repair its legacy enum sentinel. */
#include <psp2/audioout.h>
#include <stdatomic.h>
#include "utils/logger.h"

int __real_sceAudioOutOpenPort(SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode);
// Use a full integer at this ABI boundary: the SDK enum's storage type can
// narrow the required -1 sentinel back to 255, even in the repair itself.
int __real_sceAudioOutSetConfig(int port, SceSize len, int freq, int mode);
int __real_sceAudioOutOutput(int port, const void *buffer);
static atomic_uint output_calls, output_errors;

int __wrap_sceAudioOutOpenPort(SceAudioOutPortType type, int len, int freq, SceAudioOutMode mode) {
    int result = __real_sceAudioOutOpenPort(type, len, freq, mode);
    l_info("RIO_AUDIO_PORT type=%d samples=%d rate=%d mode=%d result=0x%x",
        type, len, freq, mode, result);
    return result;
}

int __wrap_sceAudioOutSetConfig(int port, SceSize len, int freq, int mode) {
    int result = __real_sceAudioOutSetConfig(port, len, freq, mode);
    // The installed OpenSL Vita.o uses an unsigned-byte enum for mode=-1.
    // Its call therefore puts 255 in r3. Only retry that precise inherit-all
    // request, and only when the original kernel call actually rejects it.
    if (result < 0 && len == (SceSize)-1 && freq == -1 && (int)mode == 255) {
        int retried = __real_sceAudioOutSetConfig(port, len, freq, -1);
        l_info("RIO_AUDIO_CONFIG inherit mode=255 result=0x%x retry_mode=-1 result=0x%x",
            result, retried);
        return retried;
    }
    l_info("RIO_AUDIO_CONFIG port=%d samples=%u rate=%d mode=%d result=0x%x",
        port, (unsigned)len, freq, mode, result);
    return result;
}

int __wrap_sceAudioOutOutput(int port, const void *buffer) {
    int result = __real_sceAudioOutOutput(port, buffer);
    atomic_fetch_add_explicit(&output_calls, 1, memory_order_relaxed);
    if (result < 0 && !atomic_fetch_add_explicit(&output_errors, 1, memory_order_relaxed))
        l_error("RIO_AUDIO_OUTPUT port=%d result=0x%x", port, result);
    return result;
}

void gangstar_audio_output_report(void) {
    l_info("RIO_AUDIO_OUTPUT calls=%u errors=%u",
        atomic_exchange(&output_calls, 0), atomic_exchange(&output_errors, 0));
}
