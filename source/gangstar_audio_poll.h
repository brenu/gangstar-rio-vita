#ifndef GANGSTAR_AUDIO_POLL_H
#define GANGSTAR_AUDIO_POLL_H
#include <stdint.h>
static inline unsigned rio_audio_poll_delay(unsigned requested, int verified_worker, int verified_call) {
    return verified_worker && verified_call && requested>8000 && requested<=33000 && requested%1000==0 ? 8000 : requested;
}
void gangstar_audio_poll_init(uintptr_t base);
void gangstar_audio_poll_worker(void);
unsigned gangstar_audio_poll_sleep(unsigned requested, uintptr_t caller);
void gangstar_audio_poll_report(void);
#endif
