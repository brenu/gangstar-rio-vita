"""Exercise actual scratch-range classification and snapshot production code."""
from pathlib import Path
import subprocess,os
src=Path('lib/vitagl/source/custom_shaders.c').read_text();a=src.index('static GLboolean rio_has_client_stream(');b=src.index('\n#endif',src.index('static GLboolean rio_snapshot_streams_impl(',a))
prefix=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define VERTEX_ATTRIBS_NUM 4
#define GL_FALSE 0
#define GL_TRUE 1
typedef int GLboolean;
typedef struct {unsigned stride;} SceGxmVertexStream;
typedef struct {unsigned format,componentCount,offset;} SceGxmVertexAttribute;
typedef struct {unsigned attr_num,attr_map[4];} program;
static struct {unsigned vertex_attrib_state;uintptr_t vertex_attrib_vbo[4];} vao,*cur_vao=&vao;
static unsigned rio_snapshot_draws;static uint64_t rio_snapshot_bytes;
static uintptr_t heap_lo,heap_hi,stack_lo=0x1000,stack_hi=0x2000;static int available=1,fail_alloc;
static void *owned[32];static unsigned allocations;
static unsigned query_ranges(uintptr_t *r,unsigned capacity){assert(capacity==2);r[0]=heap_lo;r[1]=heap_hi;r[2]=stack_lo;r[3]=stack_hi;return available?2:0;}
static unsigned (*gangstar_vertex_scratch_ranges)(uintptr_t *,unsigned)=query_ranges;
static uintptr_t extra_lo,extra_hi;static int tracker_ready=1;
static int query_extra(uintptr_t p,uintptr_t *end){if(!tracker_ready)return -1;if(extra_lo && p>=extra_lo && p<extra_hi){*end=extra_hi;return 1;}return 0;}
static int (*gangstar_vertex_extra_range)(uintptr_t,uintptr_t *)=query_extra;
static void *gpu_alloc_mapped_temp(size_t n){if(fail_alloc)return NULL;void *p=malloc(n);assert(p && allocations<32);owned[allocations++]=p;return p;}
#define vgl_fast_memcpy memcpy
'''
main=r'''
int main(void){program p={3,{2,0,1}};unsigned char scratch[512],uv[512],before[512];
 for(unsigned i=0;i<512;++i){scratch[i]=(unsigned char)i;uv[i]=(unsigned char)(i*3);}
 memcpy(before,scratch,512);heap_lo=(uintptr_t)scratch;heap_hi=heap_lo+512;
 SceGxmVertexStream streams[3]={{8},{4},{0}};SceGxmVertexAttribute attrs[3]={{9,2,0},{9,1,0},{9,1,0}};void *ptrs[3]={scratch,uv,(void *)99};
 vao.vertex_attrib_state=(1u<<2)|(1u<<0);uintptr_t ranges[4];unsigned nr;
 assert(rio_needs_vertex_snapshot(&p,ptrs,ranges,&nr));assert(ranges[0]==heap_lo && ranges[1]==heap_hi && nr==2);
 assert(rio_snapshot_streams(&p,attrs,streams,ptrs,3,10,ranges,nr));
 assert(allocations==1 && ptrs[1]==uv && ptrs[2]==(void *)99);
 assert(!memcmp(ptrs[0],before+24,80));memset(scratch,0,512);
 assert(!memcmp(ptrs[0],before+24,80));assert(rio_snapshot_bytes==80);
 // The same reusable buffer now belongs to another mesh; it needs a fresh
 // snapshot. Prior submissions must remain byte-for-byte unchanged.
 for(unsigned i=0;i<512;++i)scratch[i]=(unsigned char)(255-i);
 void *later[3]={scratch,uv,(void *)99};assert(rio_needs_vertex_snapshot(&p,later,ranges,&nr));
 assert(rio_snapshot_streams(&p,attrs,streams,later,0,8,ranges,nr));
 assert(later[0]!=ptrs[0] && !memcmp(later[0],scratch,64) && !memcmp(ptrs[0],before+24,80));
 // Allocator-independent snapshots protect two ordinary reusable arrays.
 assert(rio_has_client_stream(&p));
 assert(rio_small_client_draw(&p,attrs,streams,later,0,10));
 void *small[3]={uv,uv+16,(void *)99};
 assert(rio_snapshot_streams(&p,attrs,streams,small,0,10,ranges,0));
 unsigned char saved=((unsigned char *)small[0])[0];uv[0]^=255;
 assert(((unsigned char *)small[0])[0]==saved);uv[0]^=255;
 // Large/sparse fetch extents do not qualify just because index count is low.
 assert(!rio_small_client_draw(&p,attrs,streams,later,0,100000));
 // A VBO on one attribute must not exempt another small client array.
 vao.vertex_attrib_vbo[2]=1;assert(rio_small_client_draw(&p,attrs,streams,later,0,10));
 void *hybrid[3]={scratch,uv,(void *)99};unsigned old_allocations=allocations;
 assert(rio_snapshot_streams(&p,attrs,streams,hybrid,0,10,ranges,0));
 assert(allocations==old_allocations+1 && hybrid[0]==scratch && hybrid[1]!=uv);
 unsigned char preserved=((unsigned char *)hybrid[1])[0];uv[0]^=255;
 assert(((unsigned char *)hybrid[1])[0]==preserved);uv[0]^=255;
 vao.vertex_attrib_vbo[0]=1;assert(!rio_has_client_stream(&p) && !rio_small_client_draw(&p,attrs,streams,later,0,10));
 vao.vertex_attrib_vbo[2]=vao.vertex_attrib_vbo[0]=0;
 SceGxmVertexStream bounds_streams[3]={{1},{1},{0}};
 SceGxmVertexAttribute bounds_attrs[3]={{0,1,0},{0,1,0},{0,1,0}};
 void *budget_ptrs[3]={(void *)0x10000000,(void *)0x20000000,(void *)99};
 assert(rio_small_client_draw(&p,bounds_attrs,bounds_streams,budget_ptrs,0,32768));
 assert(!rio_small_client_draw(&p,bounds_attrs,bounds_streams,budget_ptrs,0,32769));
 assert(!rio_small_client_draw(&p,attrs,streams,later,0,0));
 // Non-scratch geometry stays on the direct path regardless of shader name.
 void *stable[3]={uv,uv+8,(void *)99};assert(!rio_needs_vertex_snapshot(&p,stable,ranges,&nr));
 void *edge[3]={(void *)heap_hi,uv,(void *)99};assert(!rio_needs_vertex_snapshot(&p,edge,ranges,&nr));
 edge[0]=(void *)(heap_hi-1);assert(rio_needs_vertex_snapshot(&p,edge,ranges,&nr));
 vao.vertex_attrib_state=0;assert(!rio_needs_vertex_snapshot(&p,later,ranges,&nr));
 vao.vertex_attrib_state=(1u<<2)|(1u<<0);
 // Missing or malformed native metadata falls back to copying every stream.
 available=0;assert(rio_needs_vertex_snapshot(&p,stable,ranges,&nr) && nr==0);
 assert(rio_snapshot_streams(&p,attrs,streams,stable,0,8,ranges,nr));assert(stable[0]!=uv && stable[1]!=uv+8);
 gangstar_vertex_scratch_ranges=NULL;assert(rio_needs_vertex_snapshot(&p,later,ranges,&nr) && nr==0);
 gangstar_vertex_scratch_ranges=query_ranges;
 available=1;heap_hi=heap_lo;assert(rio_needs_vertex_snapshot(&p,later,ranges,&nr) && nr==0);
 heap_hi=heap_lo+512;later[0]=scratch;assert(rio_needs_vertex_snapshot(&p,later,ranges,&nr));
 fail_alloc=1;void *failed[3]={scratch,uv,(void *)99};assert(!rio_snapshot_streams(&p,attrs,streams,failed,0,10,ranges,nr));assert(failed[0]==scratch);fail_alloc=0;
 assert(!rio_snapshot_streams(&p,attrs,streams,failed,0,0,ranges,nr));
 streams[0].stride=0;assert(!rio_snapshot_streams(&p,attrs,streams,failed,0,10,ranges,nr));streams[0].stride=8;
 assert(!rio_snapshot_streams(&p,attrs,streams,failed,0,65,ranges,nr)); // no heap overread
 failed[0]=NULL;assert(!rio_snapshot_streams(&p,attrs,streams,failed,0,10,ranges,0));
 failed[0]=(void *)(UINTPTR_MAX-2);assert(!rio_snapshot_streams(&p,attrs,streams,failed,0,10,ranges,0));
 unsigned char stackbuf[512];memset(stackbuf,77,sizeof(stackbuf));
 stack_lo=(uintptr_t)stackbuf;stack_hi=stack_lo+512;
 void *mixed[3]={scratch,stackbuf,(void *)99};assert(rio_needs_vertex_snapshot(&p,mixed,ranges,&nr));
 assert(rio_snapshot_streams(&p,attrs,streams,mixed,0,8,ranges,nr));
 assert(mixed[0]!=scratch && mixed[1]!=stackbuf && !memcmp(mixed[1],stackbuf,32));
 memset(stackbuf,0,512);assert(((unsigned char *)mixed[1])[0]==77);
 // Overflow allocations outside the two primary ranges are still copied.
 extra_lo=(uintptr_t)uv;extra_hi=extra_lo+512;
 void *overflowed[3]={uv,uv+16,(void *)99};assert(rio_needs_vertex_snapshot(&p,overflowed,ranges,&nr));
 assert(rio_snapshot_streams(&p,attrs,streams,overflowed,0,8,ranges,nr));assert(overflowed[0]!=uv && overflowed[1]!=uv+16);
 // Exact final-element extent: padding after the final vertex isn't read.
 streams[0].stride=16;attrs[0].componentCount=1;
 extra_hi=extra_lo+20;void *shortbuf[3]={uv,uv+64,(void *)99};
 assert(rio_needs_vertex_snapshot(&p,shortbuf,ranges,&nr));
 assert(rio_snapshot_streams(&p,attrs,streams,shortbuf,0,2,ranges,nr));
 assert(!memcmp(shortbuf[0],uv,20) && shortbuf[1]==uv+64);
 // Captured car layout: three interleaved fields exceeded the old summed
 // budget despite sharing <64KiB of actual source storage.
 program vehicle={3,{0,1,2}};vao.vertex_attrib_state=7;
 unsigned char car[32*2049];for(unsigned i=0;i<sizeof(car);++i)car[i]=(unsigned char)(i*7);
 SceGxmVertexStream cs[3]={{32},{32},{32}};
 SceGxmVertexAttribute ca[3]={{9,3,0},{9,3,0},{9,2,0}};
 void *cp[3]={car,car+12,car+24};
 assert(rio_small_client_draw(&vehicle,ca,cs,cp,0,1164));
 assert(rio_small_client_draw(&vehicle,ca,cs,cp,0,2048));
 assert(!rio_small_client_draw(&vehicle,ca,cs,cp,0,2049));
 old_allocations=allocations;uint64_t old_bytes=rio_snapshot_bytes;
 assert(rio_snapshot_streams(&vehicle,ca,cs,cp,0,1164,ranges,0));
 assert(allocations==old_allocations+1 && rio_snapshot_bytes-old_bytes==32*1164);
 assert(cp[1]==(unsigned char *)cp[0]+12 && cp[2]==(unsigned char *)cp[0]+24);
 assert(!memcmp(cp[0],car,32*1164));memset(car,99,sizeof(car));
 assert(((unsigned char *)cp[0])[0]==0 && ((unsigned char *)cp[0])[1]==7);
 void *cp2[3]={car,car+12,car+24};old_allocations=allocations;
 assert(rio_snapshot_streams(&vehicle,ca,cs,cp2,10,100,ranges,0));
 assert(allocations==old_allocations+1 && cp2[0]!=cp[0] && !memcmp(cp2[0],car+320,3200));
 assert(cp2[1]==(unsigned char *)cp2[0]+12 && cp2[2]==(unsigned char *)cp2[0]+24);
 memset(car,0,sizeof(car));assert(((unsigned char *)cp2[0])[0]==99 && ((unsigned char *)cp[0])[1]==7);
 // A byte field before a float must not destroy the float's alignment.
 program odd={2,{0,1}};vao.vertex_attrib_state=3;
 SceGxmVertexStream os[2]={{8},{8}};
 SceGxmVertexAttribute oa[2]={{9,1,0},{4,1,0}};
 void *op[2]={car+4,car+1};
 assert(rio_snapshot_streams(&odd,oa,os,op,0,4,ranges,0));
 assert(!(((uintptr_t)op[0]-(uintptr_t)(car+4))&15u));
 assert(!(((uintptr_t)op[1]-(uintptr_t)(car+1))&15u));
 assert(op[0]==(unsigned char *)op[1]+3);
 assert(!memcmp(op[0],car+4,28));
 RioVertexCopySpan unions[4];unsigned used=0;
 rio_vertex_merge_span(unions,&used,100,120,1);rio_vertex_merge_span(unions,&used,140,160,2);
 rio_vertex_merge_span(unions,&used,120,140,4);assert(used==1 && unions[0].begin==100 && unions[0].end==160 && unions[0].attributes==7);
 vao.vertex_attrib_state=(1u<<2)|(1u<<0);
 tracker_ready=0;assert(rio_needs_vertex_snapshot(&p,later,ranges,&nr) && nr==0);
 gangstar_vertex_extra_range=NULL;assert(rio_needs_vertex_snapshot(&p,later,ranges,&nr) && nr==0);
 for(unsigned i=0;i<allocations;++i)free(owned[i]);
}
'''
p=Path('work/tests/selective_vertex.c');p.write_text(prefix+src[a:b]+main)
for sanitize in (False,True):
 cmd=['cc','-std=c11','-O1' if sanitize else '-O2','-Wall','-Wextra','-Werror']
 if sanitize:cmd+=['-fsanitize=address,undefined']
 subprocess.run(cmd+[str(p),'-o','work/tests/selective_vertex'],check=True)
 subprocess.run(['work/tests/selective_vertex'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Selective vertices: actual scratch bounds, same-address mesh reuse, independent snapshots, shared car-buffer union budget/single-copy/field offsets, direct static streams, metadata fallback, disabled attributes, allocation failure and overread guards passed (ASan/UBSan)')
