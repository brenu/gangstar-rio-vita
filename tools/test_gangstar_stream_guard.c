#include "gangstar_stream_guard.h"
#include <assert.h>
#include <stdio.h>
typedef struct {void *stream;int succeeds;} File;
static int opens,queries,reports,result;
static void *backend(void *p){return ((File *)p)->stream;}
static void reopen(void *p){File *f=p;++opens;if(f->succeeds)f->stream=f;}
static int query(void *p){assert(p);++queries;return 123;}
static void report(void *p,int ready){assert(p);++reports;result=ready;}
int main(void){
 RioStreamQuery ops={backend,reopen,query,report};File f={0};
 assert(rio_stream_query(&f,&ops)==-1 && opens==1 && queries==0 && reports==1 && !result);
 f.succeeds=1;
 assert(rio_stream_query(&f,&ops)==123 && opens==2 && queries==1 && reports==2 && result);
 assert(rio_stream_query(&f,&ops)==123 && opens==2 && queries==2 && reports==2);
 puts("Stream guard: missing backend never queried, bounded reopen, failure and unchanged ready path passed");
}
