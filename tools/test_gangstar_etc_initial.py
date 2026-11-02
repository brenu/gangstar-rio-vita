"""Exercise the production base-upload allocation block with constrained pools."""
from pathlib import Path
import subprocess
s=Path('lib/vitagl/source/utils/gpu_utils.c').read_text()
a=s.index('\t\tmip_count = mip_level;',s.index('// Only this uploaded level'))
# Skip the extension case; capture only the level-zero branch.
a=s.index('\t\tmip_count = mip_level;',a+1)
b=s.index('\n\t}\n\n    upload_stage(1',a)
block=s[a:b]
code=r'''
#include <assert.h>
#include <stddef.h>
#include "etc1_mip_reserve.h"
#define MEM_ALIGNMENT 16
#define TEX_VALID 1
#define GL_OUT_OF_MEMORY 1
#define SET_GL_ERROR(e) return e;
#define upload_stage(a,b,c) ((void)0)
#define upload_clock(a) 0
static char reserved,base;
static int spare,base_ok,tries,fallbacks,freed;
static size_t wanted;
static void *gpu_alloc_mapped_aligned_for_gpu_inner(size_t alignment,size_t size){
 assert(alignment==16);++tries;wanted=size;return spare?&reserved:NULL;
}
static void *gpu_alloc_mapped_for_gpu(size_t size){assert(size==2097152);++fallbacks;return base_ok?&base:NULL;}
struct texture {int status;};
static void gpu_free_texture_data(struct texture *tex){assert(tex->status==TEX_VALID);++freed;}
static int allocate(int profile_etc,int uncompressed,int existing){
 int mip_level=0,mip_count=-1,tex_width=0,tex_height=0;
 unsigned w=2048,h=2048;int tex_size=2097152;void *texture_data;
 struct texture old={existing};struct texture *tex=&old;
''' + block + r'''
 assert(mip_count==0 && tex_width==2048 && tex_height==2048);
#ifdef VGL_ETC1_RESERVE_MIPS
 assert(texture_data==(spare && profile_etc && !uncompressed?&reserved:&base));
#else
 (void)profile_etc;(void)uncompressed;
 assert(texture_data==&base);
#endif
 return 0;
}
static void reset(void){tries=fallbacks=freed=0;wanted=0;}
int main(void){
 spare=base_ok=1;reset();assert(!allocate(1,0,1));
#ifdef VGL_ETC1_RESERVE_MIPS
 assert(tries==1 && wanted==2796216 && fallbacks==0 && freed==1);
#else
 assert(!tries && fallbacks==1 && freed==1);
#endif
 spare=0;reset();assert(!allocate(1,0,1));assert(fallbacks==1 && freed==1);
 base_ok=0;reset();assert(allocate(1,0,1)==GL_OUT_OF_MEMORY);assert(!freed);
 spare=base_ok=1;reset();assert(!allocate(0,0,0));assert(!tries && fallbacks==1 && !freed);
 reset();assert(!allocate(1,1,1));assert(!tries && fallbacks==1 && freed==1);
}
'''
out=Path('work/tests/etc_initial.c');out.parent.mkdir(parents=True,exist_ok=True);out.write_text(code)
for flags in (['-DVGL_ETC1_RESERVE_MIPS'],[]):
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Ilib/vitagl/source/utils',*flags,str(out),'-o','work/tests/etc_initial'],check=True)
 subprocess.run(['work/tests/etc_initial'],check=True)
print('ETC initial allocation: reservation, no-space fallback, OOM ownership, format guards, disabled flag passed')
