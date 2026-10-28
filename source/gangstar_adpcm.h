#ifndef GANGSTAR_ADPCM_H
#define GANGSTAR_ADPCM_H
#include <stddef.h>
#include <stdint.h>
/* Independent native block decoder. Output is interleaved signed 16-bit PCM.
   Returns frames/channel; zero means unsupported input or insufficient capacity.
   Input is immutable. No allocation, I/O, playback state or cursor changes. */
typedef struct { int16_t sample;uint8_t index; } RioImaState;
size_t rio_adpcm_ima_state(const uint8_t *,size_t,unsigned,int16_t *,size_t,RioImaState final[2]);
size_t rio_adpcm_ima(const uint8_t *,size_t,unsigned,int16_t *,size_t);
size_t rio_adpcm_ms(const uint8_t *,size_t,unsigned,const int16_t coefficients[14],int16_t *,size_t);
#endif
