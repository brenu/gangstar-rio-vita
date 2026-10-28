#include "gangstar_audio_chunk.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 assert(rio_audio_half_chunk(35280,4,1,1)==17640);
 assert(rio_audio_half_chunk(25600,4,1,1)==12800);
 assert(rio_audio_half_chunk(35280,4,0,1)==35280);
 assert(rio_audio_half_chunk(35280,4,1,0)==35280);
 for(unsigned n=0;n<9;++n)if(n!=4)assert(rio_audio_half_chunk(35280,n,1,1)==35280);
 for(unsigned n=0;n<65536;++n)if(n!=35280 && n!=25600)assert(rio_audio_half_chunk(n,4,1,1)==n);
 assert(!(rio_audio_half_chunk(35280,4,1,1)%4));
 assert(!(rio_audio_half_chunk(25600,4,1,1)%4));
 puts("Audio chunk policy: observed sizes halved/aligned; unknown stream, non-owner, ring layouts and other sizes preserved");
}
