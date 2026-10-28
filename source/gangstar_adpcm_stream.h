#ifndef GANGSTAR_ADPCM_STREAM_H
#define GANGSTAR_ADPCM_STREAM_H
#include "gangstar_adpcm.h"
typedef struct {
    unsigned channels,block_bytes,position,data_bytes,completed,total_frames;
    uint8_t *scratch;
    uint8_t state[8];
    int read_error;
} RioImaBlock;
typedef struct {
    int (*tell)(void *);int (*read)(void *,void *,unsigned);int (*seek)(void *,int);
    void *context;
} RioImaStream;
/* >=0: decoded frames; -1: original fallback with cursor untouched/restored;
   -2: rewind failed, caller must disable fast path. */
int rio_ima_read_block(RioImaBlock *,const RioImaStream *,int16_t *out,size_t capacity);
#endif
