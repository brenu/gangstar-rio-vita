// Cache relief under allocation failure, and the ETC1 reduced-base policy.
#define GANGSTAR_TEXTURE_HOT_CACHE
#define GANGSTAR_SMALL_LIGHTMAPS
#include "../source/gangstar_memory_relief.c"
#include "etc1_base_fallback.h"
#include <assert.h>
#include <stdio.h>
void _log_print(int level,const char *format,...){(void)level;(void)format;}
static size_t hot,source; static int is_main;
int gangstar_wait_is_main(void){return is_main;}
static size_t take(size_t *c,size_t n){if(n>*c)n=*c;*c-=n;return n;}
size_t gangstar_texture_hot_relieve(size_t n){assert(is_main);return take(&hot,n);}
size_t gangstar_texture_source_relieve(size_t n){return take(&source,n);}
int main(void){
 const size_t M=1024*1024;
 is_main=1;hot=12*M;source=20*M;errno=7;
 assert(gangstar_memory_relieve(2*M)==8*M && hot==4*M && source==20*M && errno==7);
 assert(gangstar_memory_relieve(9*M)==9*M && !hot && source==15*M);
 is_main=0;hot=3*M;assert(gangstar_memory_relieve(1)==8*M && hot==3*M);
 while(gangstar_memory_relieve(1)){}
 assert(!source && hot==3*M && gangstar_memory_relief_count()==4);

 vgl_etc1_base b={0};int32_t l=0;
 assert(vgl_etc1_base_map(&b,&l,2048,2048)==VGL_ETC1_UPLOAD && l==0);
 l=3;assert(vgl_etc1_base_map(&b,&l,256,256)==VGL_ETC1_UPLOAD && l==3); // untouched without OOM
 assert(!vgl_etc1_base_drop(&b,256,256));
 assert(vgl_etc1_base_drop(&b,2048,2048) && b.pending);
 l=1;assert(vgl_etc1_base_map(&b,&l,512,512)==VGL_ETC1_REJECT && b.pending);
 l=1;assert(vgl_etc1_base_map(&b,&l,1024,1024)==VGL_ETC1_UPLOAD && l==0 && b.skip==1);
 assert(vgl_etc1_base_drop(&b,1024,1024));
 l=2;assert(vgl_etc1_base_map(&b,&l,512,512)==VGL_ETC1_UPLOAD && l==0 && b.skip==2);
 b.pending=0; // stored
 for(int n=3;n<=11;++n){l=n;assert(vgl_etc1_base_map(&b,&l,2048>>n,2048>>n)==VGL_ETC1_UPLOAD && l==n-2);}
 l=1;assert(vgl_etc1_base_map(&b,&l,1024,1024)==VGL_ETC1_IGNORE);
 l=0;assert(vgl_etc1_base_map(&b,&l,2048,2048)==VGL_ETC1_UPLOAD && !b.skip);
 assert(vgl_etc1_base_drop(&b,2048,2048));
 for(int n=1;n<=5;++n){l=n;assert(vgl_etc1_base_map(&b,&l,2048>>n,2048>>n)==VGL_ETC1_UPLOAD && !l);
  assert(vgl_etc1_base_drop(&b,2048>>n,2048>>n));}
 l=6;assert(vgl_etc1_base_map(&b,&l,32,32)==VGL_ETC1_UPLOAD && !vgl_etc1_base_drop(&b,32,32) && !b.pending);
 puts("Memory relief: step/need, thread limit, exhaustion; ETC1 reduced base: remap, reject, refresh and final failure passed");
}
