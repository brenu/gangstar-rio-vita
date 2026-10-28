#ifndef GANGSTAR_AUDIO_PROBE_H
#define GANGSTAR_AUDIO_PROBE_H
#include <stdint.h>
#include <stddef.h>
enum { RIO_AUDIO_LOAD_ASYNC, RIO_AUDIO_DSP, RIO_AUDIO_PROCESS, RIO_AUDIO_FILE,
 RIO_AUDIO_RAW, RIO_AUDIO_RAM, RIO_AUDIO_CONVERT, RIO_AUDIO_LOCK, RIO_AUDIO_READ, RIO_AUDIO_MPC_DECODE, RIO_AUDIO_WAV_DECODE, RIO_AUDIO_NATIVE_DECODE, RIO_AUDIO_VORBIS_DECODE, RIO_AUDIO_SEEK, RIO_AUDIO_TELL, RIO_AUDIO_CURSOR_SEEK, RIO_AUDIO_CACHE, RIO_AUDIO_DISK, RIO_AUDIO_POSITION_QUERY, RIO_AUDIO_POSITION_SET, RIO_AUDIO_PHASES };
typedef struct { uintptr_t object; int thread; uint64_t started,elapsed,us[RIO_AUDIO_PHASES]; unsigned calls[RIO_AUDIO_PHASES]; uintptr_t decoder_target,submit_target; unsigned chunk,buffers; unsigned source_frames,source_rate,source_channels;
 uint64_t run_clocks;unsigned preemptions,releases,migrations;int cpu_valid; } RioAudioProbeEvent;
typedef struct { void *previous;int active;RioAudioProbeEvent event; } RioAudioProbeScope;
void rio_audio_scope_begin(RioAudioProbeScope *,uintptr_t object,int thread,uint64_t started);
void rio_audio_scope_end(RioAudioProbeScope *,uint64_t ended);
int rio_audio_scope_active(void);
int rio_audio_scope_matches(uintptr_t object);
void rio_audio_phase(unsigned kind,uint64_t elapsed);
void rio_audio_source(unsigned frames,unsigned rate,unsigned channels);
void rio_audio_scheduler(uint64_t clocks,unsigned preemptions,unsigned releases,unsigned migrations);
void rio_audio_stream(uintptr_t decoder,uintptr_t submit,unsigned chunk,unsigned buffers);
unsigned rio_audio_drain(RioAudioProbeEvent *out,unsigned capacity,unsigned *dropped);
#endif
