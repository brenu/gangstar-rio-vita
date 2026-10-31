from pathlib import Path
import subprocess
src=Path('lib/vitagl/source/custom_shaders.c').read_text();a=src.index('static void rio_patch_vertex(');b=src.index('\n#endif\nGLboolean _glDrawElements',a)
s='''#include "vertex_patch_cache.h"
#include <assert.h>
typedef struct {unsigned fields[4];} SceGxmVertexAttribute;
typedef struct {unsigned fields[2];} SceGxmVertexStream;
typedef struct {void *id,*prog;} shader;
typedef struct {unsigned attr_num;shader *vshader;void *vprog;} program;
static RioVertexPatchCache rio_vertex_patches;
static unsigned cur_program=7,calls;static int failure;
static void *gxm_shader_patcher=(void *)1;
static int sceGxmShaderPatcherCreateVertexProgram(void *patcher,void *id,SceGxmVertexAttribute *a,unsigned na,SceGxmVertexStream *s,unsigned ns,void **out){
 assert(patcher==(void *)1 && id==(void *)2 && a && s && na==2 && ns==2);++calls;
 if(failure)return -1;*out=(void *)99;return 0;
}
'''+src[a:b]+'''
int main(void){shader sh={(void *)2,(void *)3};program p={2,&sh,0};SceGxmVertexAttribute a[2]={0};SceGxmVertexStream s[2]={0};
 rio_patch_vertex(&p,a,s);assert(calls==1 && p.vprog==(void *)99);
 for(unsigned i=0;i<1000;++i)rio_patch_vertex(&p,a,s);assert(calls==1);
 a[1].fields[2]=7;rio_patch_vertex(&p,a,s);assert(calls==2);
 failure=1;s[0].fields[1]=20;rio_patch_vertex(&p,a,s);assert(calls==3);
 rio_patch_vertex(&p,a,s);assert(calls==4); // failed patch must never become cached
 failure=0;rio_patch_vertex(&p,a,s);rio_patch_vertex(&p,a,s);assert(calls==5);
 rio_vertex_patch_invalidate(&rio_vertex_patches,7);rio_patch_vertex(&p,a,s);assert(calls==6);
}
'''
p=Path('work/tests/vertex_patch_calls.c');p.write_text(s)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I','lib/vitagl/source/utils',str(p),'-o','work/tests/vertex_patch_calls'],check=True)
subprocess.run(['work/tests/vertex_patch_calls'],check=True)
print('Vertex patch calls: actual helper skips 1000 repeats, forwards changed layouts, retries failed patches and respects lifetime invalidation')
