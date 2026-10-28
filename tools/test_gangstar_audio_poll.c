#include "gangstar_audio_poll.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    for(unsigned us=0;us<=100000;us++) {
        assert(rio_audio_poll_delay(us,0,1)==us);
        assert(rio_audio_poll_delay(us,1,0)==us);
        unsigned expected=us>8000 && us<=33000 && us%1000==0 ? 8000 : us;
        assert(rio_audio_poll_delay(us,1,1)==expected);
    }
    assert(rio_audio_poll_delay(UINT32_MAX,1,1)==UINT32_MAX);
    puts("Audio poll cap: only verified worker/caller and native 9-33ms integer sleeps shortened");
}
