#include "gangstar_adpcm.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static const int16_t coefficients[14]={256,0,512,-256,0,0,192,64,240,0,460,-208,392,-232};
int main(void) {
 int16_t out[4096];
 uint8_t mono[8]={0,0,0,0,0x11,0x11,0x11,0x11};
 assert(rio_adpcm_ima(mono,8,1,out,4096)==9);
 for(unsigned i=0;i<9;++i)assert(out[i]==(int)i);
 uint8_t stereo[16]={0xe8,3,0,0,0x18,0xfc,0,0,0x11,0x11,0x11,0x11,0x99,0x99,0x99,0x99};
 RioImaState final[2];
 assert(rio_adpcm_ima_state(stereo,16,2,out,4096,final)==9);
 assert(final[0].sample==1008 && final[0].index==0 && final[1].sample==-1008 && final[1].index==0);
 for(unsigned i=0;i<9;++i){assert(out[2*i]==1000+(int)i);assert(out[2*i+1]==-1000-(int)i);}
 mono[0]=0xff;mono[1]=0x7f;mono[2]=88;memset(mono+4,0x77,4);
 assert(rio_adpcm_ima(mono,8,1,out,4096)==9);
 for(unsigned i=0;i<9;++i)assert(out[i]==32767);
 mono[2]=89;memset(out,0x5a,sizeof(out));assert(!rio_adpcm_ima(mono,8,1,out,4096) && out[0]==0x5a5a);
 assert(!rio_adpcm_ima(stereo,15,2,out,4096));
 assert(!rio_adpcm_ima(stereo,16,2,out,17));
 assert(!rio_adpcm_ima(stereo,16,3,out,4096));
 uint8_t ms[8]={0,16,0,100,0,50,0,0x11};
 assert(rio_adpcm_ms(ms,8,1,coefficients,out,4096)==4);
 assert(out[0]==50 && out[1]==100 && out[2]==116 && out[3]==132);
 uint8_t rounding[8]={3,16,0,255,255,0,0,0};
 assert(rio_adpcm_ms(rounding,8,1,coefficients,out,4096)==4);
 assert(out[0]==0 && out[1]==-1 && out[2]==-1 && out[3]==-1);
 uint8_t ms_stereo[15]={0,0,16,0,32,0,100,0,156,255,50,0,206,255,0x1f};
 assert(rio_adpcm_ms(ms_stereo,15,2,coefficients,out,4096)==3);
 assert(out[0]==50 && out[1]==-50 && out[2]==100 && out[3]==-100 && out[4]==116 && out[5]==-132);
 ms[0]=7;memset(out,0x5a,sizeof(out));assert(!rio_adpcm_ms(ms,8,1,coefficients,out,4096) && out[0]==0x5a5a);
 assert(!rio_adpcm_ms(ms_stereo,15,2,coefficients,out,5));
 assert(!rio_adpcm_ms(ms_stereo,13,2,coefficients,out,4096));
 assert(!rio_adpcm_ms(NULL,8,1,coefficients,out,4096));
 assert(!rio_adpcm_ima(NULL,8,1,out,4096));
 puts("Native ADPCM: hand-calculated mono/stereo, nibble order, interleaving, clipping, original signed rounding, predictor/index and capacity guards passed");
}
