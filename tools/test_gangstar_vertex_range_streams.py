from pathlib import Path
import subprocess
s=Path('lib/vitagl/source/custom_shaders.c').read_text()
a=s.index('#define disable_draw_attrib(i)');b=s.index('#ifndef HAVE_FFP_SHADER_SUPPORT',a)
# Also execute the exact packed-client block from the ranged implementation.
a2=s.index('GLboolean _glDrawElements_CustomShadersRangeIMPL(')
b2=s.index('\n#ifdef STRICT_DRAW_COMPLIANCE\n\t// Gathering real attribute data pointers',a2)
e2=s.index('\n#else\n\t// Gathering real attribute data pointers',b2)
packed=s[b2+len('\n#ifdef STRICT_DRAW_COMPLIANCE'):e2]
prefix=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define SAFE_DRAW_SIZE_THRESHOLD 32768
#define GL_FALSE 0
#define GL_TRUE 1
#define SCE_GXM_ATTRIBUTE_FORMAT_F32 0
typedef struct {unsigned char *ptr;unsigned last_frame;} vbo;
typedef struct {unsigned stride;} Stream;
typedef struct {unsigned offset,format,componentCount,regIndex;} Attribute;
static struct {unsigned vertex_attrib_state;void *vertex_attrib_vbo[4];uintptr_t vertex_attrib_offsets[4];unsigned vertex_attrib_size[4];} vao,*cur_vao=&vao;
static struct {unsigned regIndex;} attr[4];
static struct {unsigned attr_map[4];int attr_num;typeof(attr[0]) *attr;} prog,*p=&prog;
static unsigned vgl_framecount=5,orig_stride[4],orig_fmt[4],orig_size[4];
static unsigned char source[65536],pool[65536];static unsigned allocations;
static void *gpu_alloc_mapped_temp(size_t n){assert(n<=sizeof(pool));++allocations;return pool;}
#define vgl_fast_memcpy memcpy
'''
main=r'''
static void run(unsigned vertices,unsigned first_vertex){
 Stream streams[4]={{8},{8}};Attribute attributes[4]={{0}};void *ptrs[4]={0};
 unsigned top_idx=vertices;unsigned char is_packed[4]={1,1};
 prog.attr_num=2;prog.attr=attr;prog.attr_map[0]=0;prog.attr_map[1]=1;
 cur_vao->vertex_attrib_state=3;cur_vao->vertex_attrib_offsets[0]=(uintptr_t)source;
 cur_vao->vertex_attrib_offsets[1]=(uintptr_t)(source+4);
'''+packed+r'''
 assert(attributes[0].offset==0 && attributes[1].offset==4);
#ifdef SAFER_DRAW_SPEEDHACK
 if(vertices*8>32768)assert(ptrs[0]==source+first_vertex*8);
 else
#endif
 {assert(ptrs[0]==pool && !memcmp(pool,source+first_vertex*8,vertices*8));}
 // Exercise the actual separate-stream macro with the same source shift.
 unsigned i=0,attr_idx=0;streams[0].stride=8;attributes[0].offset=0;
 handle_unpacked_attrib(first_vertex,top_idx);
#ifdef SAFER_DRAW_SPEEDHACK
 if(vertices*8>32768)assert(ptrs[0]==source+first_vertex*8);
 else
#endif
 {assert(ptrs[0]==pool && !memcmp(pool,source+first_vertex*8,vertices*8));}
 // Disabled constants retain stride zero rather than receiving a source shift.
 cur_vao->vertex_attrib_state=0;cur_vao->vertex_attrib_size[0]=4;
 handle_unpacked_attrib(first_vertex,top_idx);assert(streams[0].stride==0 && attributes[0].componentCount==4);
}
static void direct_streams(void) {
 Stream streams[4]={{8},{8}};Attribute attributes[4]={{0}};void *ptrs[4]={0};
 prog.attr_num=2;prog.attr=attr;prog.attr_map[0]=0;prog.attr_map[1]=1;
 cur_vao->vertex_attrib_state=3;
 cur_vao->vertex_attrib_vbo[0]=cur_vao->vertex_attrib_vbo[1]=0;
 cur_vao->vertex_attrib_offsets[0]=(uintptr_t)source;
 cur_vao->vertex_attrib_offsets[1]=(uintptr_t)(source+4);
 unsigned before=allocations;
 handle_speedhack_attrib();
 assert(allocations==before && ptrs[0]==source && ptrs[1]==source+4);
 assert(attributes[0].offset==0 && attributes[1].offset==0);
 for(unsigned v=0;v<100;++v)assert(!memcmp((unsigned char *)ptrs[1]+v*8,source+4+v*8,4));
 vbo vb={source,0};cur_vao->vertex_attrib_vbo[1]=&vb;cur_vao->vertex_attrib_offsets[1]=4;
 handle_speedhack_attrib();assert(ptrs[1]==source+4 && vb.last_frame==vgl_framecount);
 cur_vao->vertex_attrib_state=1;cur_vao->vertex_attrib_size[1]=4;
 handle_speedhack_attrib();assert(streams[1].stride==0 && attributes[1].componentCount==4);
}
int main(void){for(unsigned i=0;i<sizeof(source);++i)source[i]=(unsigned char)(i^(i>>8));
 run(3,5000);run(5000,300);run(4096,256);run(4097,256);
 assert(allocations>0);direct_streams();}
'''
for flag in (False,True):
 path=Path('work/tests/vertex_range_streams.c');path.write_text(prefix+s[a:b]+main)
 cmd=['cc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-I','lib/vitagl/source/utils']
 if flag:cmd+=['-DSAFER_DRAW_SPEEDHACK']
 subprocess.run(cmd+[str(path),'-o','work/tests/vertex_range_streams'],check=True)
 subprocess.run(['work/tests/vertex_range_streams'],check=True)
print('Vertex range streams: actual packed block and separate-stream macros preserve bytes and offsets with speedhack OFF/ON, threshold boundaries, disabled constants and full direct-stream path')
