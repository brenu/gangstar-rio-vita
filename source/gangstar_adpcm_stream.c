#include "gangstar_adpcm_stream.h"
int rio_ima_read_block(RioImaBlock *b,const RioImaStream *s,int16_t *out,size_t capacity) {
    unsigned channels=b->channels;
    b->read_error=0;
    if(channels<1 || channels>2 || b->block_bytes!=1024 || !b->scratch || !out ||
        b->position>b->data_bytes || b->completed>b->total_frames)return -1;
    unsigned wanted=b->data_bytes-b->position;if(wanted>1024)wanted=1024;
    if(wanted<4*channels || (wanted-4*channels)%(4*channels) ||
        1+(wanted-4*channels)*2/channels>capacity/channels)return -1;
    int before=s->tell(s->context);if(before<0)return -1;
    int read=s->read(s->context,b->scratch,wanted);
    if(read<=0){b->read_error=read<0;return 0;}
    RioImaState final[2];
    size_t frames=read<=(int)wanted?rio_adpcm_ima_state(b->scratch,(size_t)read,channels,out,capacity,final):0;
    if(!frames) {
        s->seek(s->context,before);
        if(s->tell(s->context)!=before)return -2;
        return -1;
    }
    for(unsigned c=0;c<channels;++c) {
        uint16_t sample=(uint16_t)final[c].sample;
        b->state[4*c]=(uint8_t)sample;b->state[4*c+1]=(uint8_t)(sample>>8);
        b->state[4*c+2]=final[c].index;b->state[4*c+3]=b->scratch[4*c+3];
    }
    b->position+=(unsigned)read;
    if(frames>b->total_frames-b->completed)frames=b->total_frames-b->completed;
    return (int)frames;
}
