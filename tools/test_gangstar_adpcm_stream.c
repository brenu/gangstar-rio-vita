#include "gangstar_adpcm_stream.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct Source { unsigned position,reads,seeks;int limit,fail_seek,fail_read,bad_tell;uint8_t data[1024]; };
static int tell(void *p){struct Source *s=p;return s->bad_tell?-1:(int)s->position;}
static int read(void *p,void *out,unsigned n){struct Source *s=p;++s->reads;if(s->fail_read)return -1;if(s->limit>=0 && n>(unsigned)s->limit)n=(unsigned)s->limit;memcpy(out,s->data,n);s->position+=n;return (int)n;}
static int seek(void *p,int pos){struct Source *s=p;++s->seeks;if(s->fail_seek)return -1;s->position=(unsigned)pos;return 0;}
int main(void) {
 struct Source s={.limit=-1};s.data[3]=0x7a;
 uint8_t scratch[1024];int16_t out[2048];
 RioImaStream ops={tell,read,seek,&s};
 RioImaBlock b={.channels=1,.block_bytes=1024,.data_bytes=1024,.total_frames=2041,.scratch=scratch};
 assert(rio_ima_read_block(&b,&ops,out,2048)==2041);
 assert(b.position==1024 && s.position==1024 && b.state[3]==0x7a && !s.seeks);
 b.position=0;b.completed=100;b.total_frames=105;s.position=0;
 assert(rio_ima_read_block(&b,&ops,out,2048)==5 && b.position==1024); /* Native fact trimming. */
 b.position=0;b.completed=0;b.total_frames=2041;s.position=0;s.data[2]=99;
 assert(rio_ima_read_block(&b,&ops,out,2048)==-1 && !b.position && !s.position && s.seeks==1);
 s.fail_seek=1;assert(rio_ima_read_block(&b,&ops,out,2048)==-2 && !b.position);
 s=(struct Source){.limit=7};assert(rio_ima_read_block(&b,&ops,out,2048)==-1 && !s.position && !b.position);
 s=(struct Source){.limit=-1,.fail_read=1};assert(rio_ima_read_block(&b,&ops,out,2048)==0 && b.read_error && !b.position);
 s=(struct Source){.limit=0};assert(rio_ima_read_block(&b,&ops,out,2048)==0 && !b.read_error);
 s=(struct Source){.limit=-1,.bad_tell=1};assert(rio_ima_read_block(&b,&ops,out,2048)==-1 && !s.reads);
 s=(struct Source){.limit=-1};b.block_bytes=512;assert(rio_ima_read_block(&b,&ops,out,2048)==-1 && !s.reads);
 b.block_bytes=1024;assert(rio_ima_read_block(&b,&ops,out,3)==-1 && !s.reads);
 b.position=1025;assert(rio_ima_read_block(&b,&ops,out,2048)==-1 && !s.reads);
 puts("Native IMA stream: byte/state advancement, fact trimming, reserved byte, malformed/short-read rewind, seek failure, read error/EOF and untouched fallback passed");
}
