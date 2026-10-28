#include "gangstar_adpcm.h"
static const int steps[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const int index_delta[8]={-1,-1,-1,-1,2,4,6,8};
static const int adaptation[16]={230,230,230,230,307,409,512,614,768,614,512,409,307,230,230,230};
static unsigned le16(const uint8_t *p) { return (unsigned)p[0]|(unsigned)p[1]<<8; }
static int signed16(unsigned n) { return n<32768?(int)n:(int)n-65536; }
static int clip(int n) { return n>32767?32767:n< -32768?-32768:n; }
static inline int ima_nibble(unsigned n,int *sample,int *index) {
    int step=steps[*index],difference=step>>3;
    if(n&4)difference+=step;
    if(n&2)difference+=step>>1;
    if(n&1)difference+=step>>2;
    *sample=clip(*sample+((n&8)?-difference:difference));
    int next=*index+index_delta[n&7];*index=next<0?0:next>88?88:next;
    return *sample;
}
size_t rio_adpcm_ima_state(const uint8_t *in,size_t bytes,unsigned channels,int16_t *out,size_t capacity,RioImaState final[2]) {
    if(!in || !out || channels<1 || channels>2 || bytes<4*channels)return 0;
    size_t payload=bytes-4*channels;
    if(payload%(4*channels) || payload> (SIZE_MAX-1)/2)return 0;
    size_t frames=1+payload*2/channels;
    if(frames>capacity/channels)return 0;
    int sample[2],index[2];
    for(unsigned c=0;c<channels;++c) {
        sample[c]=signed16(le16(in+4*c));index[c]=in[4*c+2];
        if(index[c]>88)return 0;
    }
    for(unsigned c=0;c<channels;++c)out[c]=(int16_t)sample[c];
    size_t at=4*channels,frame=1;
    while(at<bytes) {
        for(unsigned c=0;c<channels;++c)for(unsigned j=0;j<4;++j) {
            unsigned b=in[at++];
            out[(frame+2*j)*channels+c]=(int16_t)ima_nibble(b&15,&sample[c],&index[c]);
            out[(frame+2*j+1)*channels+c]=(int16_t)ima_nibble(b>>4,&sample[c],&index[c]);
        }
        frame+=8;
    }
    if(final)for(unsigned c=0;c<channels;++c)final[c]=(RioImaState){(int16_t)sample[c],(uint8_t)index[c]};
    return frames;
}
size_t rio_adpcm_ima(const uint8_t *in,size_t bytes,unsigned channels,int16_t *out,size_t capacity) {
    return rio_adpcm_ima_state(in,bytes,channels,out,capacity,NULL);
}
struct MSState { int sample1,sample2,delta,coef1,coef2; };
static inline int ms_nibble(unsigned n,struct MSState *s) {
    int sum=s->sample1*s->coef1+s->sample2*s->coef2;
    /* Original ARM ASR rounds negative predictors down, rather than toward zero. */
    int prediction=sum>=0?sum/256:-((-sum+255)/256);
    int signed_n=n<8?(int)n:(int)n-16;
    int sample=clip(prediction+signed_n*s->delta);
    s->sample2=s->sample1;s->sample1=sample;
    int delta=adaptation[n]*s->delta/256;
    s->delta=delta<16?16:(int)(uint16_t)delta; /* Original stores unsigned 16-bit delta. */
    return sample;
}
size_t rio_adpcm_ms(const uint8_t *in,size_t bytes,unsigned channels,const int16_t coefficients[14],int16_t *out,size_t capacity) {
    if(!in || !out || !coefficients || channels<1 || channels>2 || bytes<7*channels)return 0;
    size_t payload=bytes-7*channels;
    if(payload>(SIZE_MAX-2)/2)return 0;
    size_t frames=2+payload*2/channels;
    if(frames>capacity/channels)return 0;
    struct MSState state[2];
    for(unsigned c=0;c<channels;++c) {
        unsigned predictor=in[c];if(predictor>=7)return 0;
        int a=coefficients[2*predictor],b=coefficients[2*predictor+1];
        /* Bounds keep predictor arithmetic defined even for unsupported tables. */
        if(a< -512 || a>512 || b< -512 || b>512)return 0;
        state[c]=(struct MSState){signed16(le16(in+3*channels+2*c)),
            signed16(le16(in+5*channels+2*c)),(int)le16(in+channels+2*c),a,b};
    }
    for(unsigned c=0;c<channels;++c) { out[c]=(int16_t)state[c].sample2;out[channels+c]=(int16_t)state[c].sample1; }
    size_t frame=2;
    for(size_t at=7*channels;at<bytes;++at) {
        unsigned b=in[at];
        if(channels==2) {
            out[frame*2]=(int16_t)ms_nibble(b>>4,&state[0]);
            out[frame*2+1]=(int16_t)ms_nibble(b&15,&state[1]);++frame;
        } else {
            out[frame++]=(int16_t)ms_nibble(b>>4,&state[0]);
            out[frame++]=(int16_t)ms_nibble(b&15,&state[0]);
        }
    }
    return frames;
}
