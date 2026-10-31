#include "gangstar_vertex_watch.h"
#include <assert.h>
#include <stdlib.h>
int main(void) {
 static RioVertexWatch w;unsigned char *source=malloc(64);assert(source);
 memset(source,17,64);w.begin=(uintptr_t)source;w.end=w.begin+64;
 rio_vertex_watch_add(&w,"Vertex",3,9,source,12);
 rio_vertex_watch_add(&w,"texcoord1",3,9,source+16,8);
 assert(w.count==2 && source[0]==17);
 rio_vertex_watch_add(&w,"outside",0,0,(void *)(w.end-1),16);
 rio_vertex_watch_add(&w,"wrap",0,0,(void *)(UINTPTR_MAX-1),16);
 assert(w.count==2);source[0]=23;
 FILE *f=tmpfile();assert(f);rio_vertex_watch_report(&w,f,1);rewind(f);
 char text[2048];size_t n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);
 assert(strstr(text,"source_changed draw=3 attribute=Vertex index=9"));
 assert(strstr(text,"samples=2 changed=1 dropped=0 range_valid=1"));assert(!w.count);
 for(unsigned i=0;i<RIO_VERTEX_WATCH_CAPACITY+3;++i)rio_vertex_watch_add(&w,"Vertex",1,0,source,12);
 assert(w.count==RIO_VERTEX_WATCH_CAPACITY && w.dropped==3);
 free(source);f=tmpfile();assert(f);rio_vertex_watch_report(&w,f,0);rewind(f);
 n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);
 assert(strstr(text,"changed=0 dropped=3 range_valid=0"));assert(!w.count && !w.dropped);
 puts("Vertex watch: exact mutations, immutable samples, bounds, wrap, bounded capacity, report reset and no reads after invalidation passed");
}
