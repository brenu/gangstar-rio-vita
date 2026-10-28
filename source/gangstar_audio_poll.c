#include "gangstar_audio_poll.h"
#include "utils/logger.h"
#include <stdatomic.h>
#include <string.h>
static uintptr_t sleep_caller;
static _Thread_local int emitter_worker;
static atomic_uint capped, requested_saved;
void gangstar_audio_poll_init(uintptr_t base) {
#ifdef GANGSTAR_AUDIO_POLL_CAP
    /* ARM Sleep(ms) tail-calls imported usleep; LR stays at its loop caller. */
    static const uint32_t sleep_code[]={0xe3a03ffa,0xe0000093,0xeae899c7};
    static const uint32_t call_code=0xeb000013;
    if (!memcmp((void *)(base+0x94a828),sleep_code,sizeof(sleep_code)) &&
        !memcmp((void *)(base+0x94a7d4),&call_code,sizeof(call_code))) sleep_caller=base+0x94a7d8;
    l_info("RIO_AUDIO_POLL ready=%d cap_us=8000",sleep_caller!=0);
#else
    (void)base;
#endif
}
void gangstar_audio_poll_worker(void) { emitter_worker=1; }
unsigned gangstar_audio_poll_sleep(unsigned requested, uintptr_t caller) {
    unsigned actual=rio_audio_poll_delay(requested,emitter_worker,sleep_caller && caller==sleep_caller);
    if(actual!=requested) {
        atomic_fetch_add_explicit(&capped,1,memory_order_relaxed);
        atomic_fetch_add_explicit(&requested_saved,requested-actual,memory_order_relaxed);
    }
    return actual;
}
void gangstar_audio_poll_report(void) {
    l_info("RIO_AUDIO_POLL capped=%u requested_sleep_removed_us=%u",
        atomic_exchange(&capped,0),atomic_exchange(&requested_saved,0));
}
