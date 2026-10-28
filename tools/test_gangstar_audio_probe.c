#include "gangstar_audio_probe.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
static void *worker(void *p) {
    (void)p;assert(!rio_audio_scope_active());
    RioAudioProbeScope s;rio_audio_scope_begin(&s,22,2,10);
    rio_audio_phase(RIO_AUDIO_DSP,4000);rio_audio_scope_end(&s,6010);
    assert(!rio_audio_scope_active());return NULL;
}
int main(void) {
    assert(!rio_audio_scope_active());
    RioAudioProbeScope s,nested;errno=EINTR;
    rio_audio_scope_begin(&s,11,1,100);assert(errno==EINTR && rio_audio_scope_active());
    assert(rio_audio_scope_matches(11) && !rio_audio_scope_matches(99));
    rio_audio_phase(RIO_AUDIO_LOAD_ASYNC,7000);
    rio_audio_phase(RIO_AUDIO_CACHE,650);rio_audio_phase(RIO_AUDIO_DISK,300);
    rio_audio_phase(RIO_AUDIO_POSITION_QUERY,10);rio_audio_phase(RIO_AUDIO_POSITION_SET,20);
    rio_audio_stream(0x1234,0x5678,16384,3);
    rio_audio_phase(RIO_AUDIO_VORBIS_DECODE,3000);
    rio_audio_phase(RIO_AUDIO_SEEK,123);rio_audio_phase(RIO_AUDIO_TELL,45);
    rio_audio_source(12000,32000,2);rio_audio_scheduler(500,3,4,1);
    pthread_t t;assert(!pthread_create(&t,NULL,worker,NULL));assert(!pthread_join(t,NULL));
    rio_audio_scope_begin(&nested,33,1,200);rio_audio_phase(RIO_AUDIO_READ,800);
    rio_audio_scope_end(&nested,300);assert(rio_audio_scope_active() && rio_audio_scope_matches(11)); /* Fast nested event omitted. */
    rio_audio_phase(RIO_AUDIO_LOCK,6000);
    rio_audio_scope_end(&s,10100);
    assert(!rio_audio_scope_active());
    RioAudioProbeEvent e[64];unsigned dropped;
    unsigned count=rio_audio_drain(e,64,&dropped);assert(count==2 && !dropped);
    assert(e[0].object==22 && e[0].thread==2 && e[0].us[RIO_AUDIO_DSP]==4000);
    assert(e[1].object==11 && e[1].elapsed==10000 && e[1].us[RIO_AUDIO_LOAD_ASYNC]==7000);
    assert(e[1].decoder_target==0x1234 && e[1].submit_target==0x5678 && e[1].chunk==16384 && e[1].buffers==3);
    assert(e[1].us[RIO_AUDIO_VORBIS_DECODE]==3000);
    assert(e[1].us[RIO_AUDIO_SEEK]==123 && e[1].calls[RIO_AUDIO_SEEK]==1 && e[1].us[RIO_AUDIO_TELL]==45);
    assert(e[1].source_frames==12000 && e[1].source_rate==32000 && e[1].source_channels==2);
    assert(e[1].cpu_valid && e[1].run_clocks==500 && e[1].preemptions==3 && e[1].releases==4 && e[1].migrations==1);
    assert(e[1].us[RIO_AUDIO_CACHE]==650 && e[1].us[RIO_AUDIO_DISK]==300 && e[1].calls[RIO_AUDIO_DISK]==1);
    assert(e[1].us[RIO_AUDIO_POSITION_QUERY]==10 && e[1].us[RIO_AUDIO_POSITION_SET]==20);
    assert(!e[0].cpu_valid && !e[0].source_frames);
    assert(e[1].us[RIO_AUDIO_LOCK]==6000 && !e[1].us[RIO_AUDIO_READ]);
    for(unsigned i=0;i<70;++i) { rio_audio_scope_begin(&s,i,1,1);rio_audio_scope_end(&s,6001); }
    assert(rio_audio_drain(e,64,&dropped)==64 && dropped==6);
    assert(!rio_audio_drain(e,64,&dropped) && !dropped);
    puts("Audio update profiler: thread isolation, nested restoration, phase totals, fast-event exclusion, bounded queue and drops passed");
}
