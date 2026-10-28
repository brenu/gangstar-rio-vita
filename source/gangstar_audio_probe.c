#include "gangstar_audio_probe.h"
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
static pthread_once_t once=PTHREAD_ONCE_INIT;
static pthread_key_t key;
static atomic_int ready;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static RioAudioProbeEvent events[64];
static unsigned count,dropped;
static void create_key(void) { if(!pthread_key_create(&key,NULL))atomic_store(&ready,1); }
int rio_audio_scope_active(void) { return atomic_load(&ready) && pthread_getspecific(key)!=NULL; }
int rio_audio_scope_matches(uintptr_t object) {
    if(!atomic_load(&ready))return 0;
    RioAudioProbeScope *scope=pthread_getspecific(key);
    return scope && scope->event.object==object;
}
void rio_audio_scope_begin(RioAudioProbeScope *scope,uintptr_t object,int thread,uint64_t started) {
    int saved=errno;
    *scope=(RioAudioProbeScope){0};pthread_once(&once,create_key);
    if(atomic_load(&ready)) {
        scope->previous=pthread_getspecific(key);
        scope->event.object=object;scope->event.thread=thread;scope->event.started=started;
        scope->active=pthread_setspecific(key,scope)==0;
    }
    errno=saved;
}
void rio_audio_phase(unsigned kind,uint64_t elapsed) {
    if(kind>=RIO_AUDIO_PHASES || !atomic_load(&ready))return;
    RioAudioProbeScope *scope=pthread_getspecific(key);
    if(scope) { scope->event.us[kind]+=elapsed;++scope->event.calls[kind]; }
}
void rio_audio_source(unsigned frames,unsigned rate,unsigned channels) {
    if(!atomic_load(&ready))return;
    RioAudioProbeScope *scope=pthread_getspecific(key);
    if(scope) { scope->event.source_frames=frames;scope->event.source_rate=rate;scope->event.source_channels=channels; }
}
void rio_audio_scheduler(uint64_t clocks,unsigned preemptions,unsigned releases,unsigned migrations) {
    if(!atomic_load(&ready))return;
    RioAudioProbeScope *scope=pthread_getspecific(key);
    if(scope) {
        scope->event.run_clocks=clocks;scope->event.preemptions=preemptions;
        scope->event.releases=releases;scope->event.migrations=migrations;scope->event.cpu_valid=1;
    }
}
void rio_audio_stream(uintptr_t decoder,uintptr_t submit,unsigned chunk,unsigned buffers) {
    if(!atomic_load(&ready))return;
    RioAudioProbeScope *scope=pthread_getspecific(key);
    if(scope) {
        scope->event.decoder_target=decoder;scope->event.submit_target=submit;
        scope->event.chunk=chunk;scope->event.buffers=buffers;
    }
}
void rio_audio_scope_end(RioAudioProbeScope *scope,uint64_t ended) {
    if(!scope->active)return;
    int saved=errno;
    pthread_setspecific(key,scope->previous);
    scope->event.elapsed=ended-scope->event.started;
    if(scope->event.elapsed>=5000) {
        pthread_mutex_lock(&lock);
        if(count<64)events[count++]=scope->event;else ++dropped;
        pthread_mutex_unlock(&lock);
    }
    errno=saved;
}
unsigned rio_audio_drain(RioAudioProbeEvent *out,unsigned capacity,unsigned *lost) {
    pthread_mutex_lock(&lock);
    unsigned n=count<capacity?count:capacity;
    for(unsigned i=0;i<n;++i)out[i]=events[i];
    for(unsigned i=n;i<count;++i)events[i-n]=events[i];
    count-=n;*lost=dropped;dropped=0;
    pthread_mutex_unlock(&lock);
    return n;
}
