"""Exercise the actual indexed-draw integration, with GXM calls mocked."""
from pathlib import Path
import subprocess
s=Path('lib/vitagl/source/draw.c').read_text();a=s.index('void glDrawElements(');b=s.index('\nvoid glDrawElementsBaseVertex',a)
prefix=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vertex_range.h"
#define RIO_SELECTIVE_VERTEX_COPY
#define RIO_VERTEX_RANGE_COPY
#define SKIP_ERROR_HANDLING
#define THREAD_SAFE()
#define GL_UNSIGNED_SHORT 1
#define GL_UNSIGNED_INT 2
#define GL_UNSIGNED_BYTE 3
#define GL_TRIANGLES 4
#define GL_TRUE 1
#define SCE_GXM_INDEX_SOURCE_INDEX_16BIT 0
#define SCE_GXM_INDEX_SOURCE_INDEX_32BIT 1
#define SCE_GXM_INDEX_FORMAT_U16 0
#define SCE_GXM_INDEX_FORMAT_U32 1
#define RIO_STAGE_BEGIN() 0
#define RIO_STAGE_END(a,b) ((void)(b))
#define gl_primitive_to_gxm(mode,p,count) p=mode
#define restore_polygon_mode(p) ((void)(p))
typedef unsigned GLenum;typedef int GLsizei,GLboolean,SceGxmPrimitiveType;typedef void GLvoid;
typedef struct {void *ptr;unsigned last_frame;} vbo;
static struct {vbo *index_array_unit;} vao,*cur_vao=&vao;
static unsigned cur_program=1,vgl_framecount=1,ffp_vertex_attrib_state=1;
static int prim_is_non_native,gxm_context,eligible=1,fail_alloc,setup_ok=1;
static unsigned rio_range_draws,rio_range_trimmed;static uint64_t rio_range_saved;
static uint32_t captured_first,captured_span;static unsigned captured_calls,draws;
static uint16_t drawn[16];static void *allocated[32];static unsigned allocations;
static void scene_reset(void){}
static int vglRioClientRangeEligible(void){return eligible;}
static void *gpu_alloc_mapped_temp(size_t size){if(fail_alloc)return NULL;void *p=malloc(size);assert(p);allocated[allocations++]=p;return p;}
static int _glDrawElements_CustomShadersRangeIMPL(uint16_t *src,int count,uint32_t span,uint32_t base,unsigned type,uint32_t first){
 assert(src && count>0 && !base);(void)type;captured_first=first;captured_span=span;++captured_calls;return setup_ok;
}
static void _glDrawElements_FixedFunctionIMPL(uint16_t *src,int count,unsigned a,unsigned b,unsigned type){(void)src;(void)count;(void)a;(void)b;(void)type;}
static void sceGxmDraw(int ctx,unsigned mode,unsigned type,void *p,int count){(void)ctx;(void)mode;assert(type==0);assert(count<=16);memcpy(drawn,p,count*2);++draws;}
#define setup_elements_indices(t) t *ptr=src
#define setup_8bit_elements_indices() uint16_t *ptr=src
'''
main=r'''
int main(void){
 uint16_t indices[]={5002,5000,5001},original[3];memcpy(original,indices,sizeof(indices));
 glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,indices);
 assert(captured_first==5000 && captured_span==3 && rio_range_trimmed==1 && rio_range_saved==5000);
 assert(drawn[0]==2 && drawn[1]==0 && drawn[2]==1 && !memcmp(indices,original,sizeof(indices)));
 eligible=0;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,indices);assert(!captured_first && !captured_span && drawn[0]==5002);
 eligible=1;fail_alloc=1;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,indices);assert(!captured_first && captured_span==5003 && drawn[0]==5002);
 fail_alloc=0;uint16_t low[]={0,1,2};glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,low);assert(!captured_first && captured_span==3);
 glDrawElements(99,3,GL_UNSIGNED_SHORT,indices);assert(!captured_first);
 vbo ibo={indices,0};vao.index_array_unit=&ibo;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,0);assert(!captured_first);
 assert(captured_calls==6 && draws==6 && rio_range_trimmed==1);
 // NO_DEBUG must not submit stale GXM streams after an internal failure.
 vao.index_array_unit=NULL;setup_ok=0;
 glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,low);
 assert(draws==6 && !captured_first);
 glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,indices);
 assert(draws==6 && captured_first==5000);
 unsigned rejected=0;vglGetRejectedDrawSetupStats(&rejected);assert(rejected==2);
 vglGetRejectedDrawSetupStats(&rejected);assert(!rejected);
 setup_ok=1;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,low);assert(draws==7);
 cur_program=0;glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,low);assert(draws==8);
 for(unsigned i=0;i<allocations;++i)free(allocated[i]);
}
'''
h=s.index('#ifdef RIO_SELECTIVE_VERTEX_COPY\nstatic unsigned rio_rejected_draw_setups');end=s.index('#ifndef INDICES_DRAW_SPEEDHACK',h)
assert s.count('if (rio_draw_setup_ready(is_draw_legal))')==7
p=Path('work/tests/vertex_range_draw.c');p.write_text(prefix+s[h:end]+s[a:b]+main)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-but-set-variable','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-incompatible-pointer-types','-I','lib/vitagl/source/utils',str(p),'-o','work/tests/vertex_range_draw'],check=True)
subprocess.run(['work/tests/vertex_range_draw'],check=True)
print('Vertex range draw: actual production draw forwards rebased indices, preserves sources, and falls back for mixed streams, allocation failure, low ranges, other modes and IBOs; NO_DEBUG blocks failed raw/rebased setups, counts rejection and recovers')
