"""Compile the actual prefix policy and injected-error branch, with both flags."""
from pathlib import Path
import subprocess
s=Path('lib/vitagl/source/utils/gpu_utils.c').read_text()
a=s.index('static int gpu_keep_etc_prefix(');b=s.index('\nvoid gpu_alloc_compressed_texture(',a)
helper=s[a:b]
a=s.index('#ifdef VGL_UPLOAD_STAGES\n    // Explicit one-shot')
b=s.index('\n\tint mip_count, tex_width, tex_height;',a)
# Adapt only the void success return to an integer result for the harness.
branch=s[a:b].replace(') return;', ') return 0;')
code=r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define TEX_VALID 1
#define SCE_GXM_TEXTURE_FORMAT_ETC1_1BGR 9
#define GL_OUT_OF_MEMORY 0x505
#define SET_GL_ERROR(e) return e;
typedef unsigned SceGxmTextureFormat;
typedef struct { unsigned format,count,untouched; } Descriptor;
typedef struct { int status; void *data; uint8_t mip_count,use_mips,compressed_mip_failed; Descriptor gxm_tex; } texture;
static unsigned fallback_records,errors;
static int inject;
static unsigned vglGetTexFormat(const Descriptor *d){return d->format;}
static void vglSetTexMipmapCount(Descriptor *d,unsigned count){d->count=count;}
void gangstar_texture_prefix_fallback(const void *p,int level,unsigned kept){assert(p && level>=1 && kept);++fallback_records;}
int gangstar_texture_fault_inject(const void *p,int l,unsigned w,unsigned h) __attribute__((weak));
int gangstar_texture_fault_inject(const void *p,int l,unsigned w,unsigned h){(void)p;(void)l;(void)w;(void)h;return inject;}
void gangstar_texture_alloc_failure(const void *,int,unsigned,unsigned,size_t) __attribute__((weak));
void gangstar_texture_alloc_failure(const void *p,int l,unsigned w,unsigned h,size_t n){assert(p && l>=0 && w && h && n);++errors;}
''' + helper + r'''
static int attempt(texture *tex,int mip_level,int uncompressed) {
 unsigned max_width=2048,max_height=2048,w=64,h=64;int tex_size=2795520;
 SceGxmTextureFormat format=9;int profile_etc=1;
''' + branch + r'''
 return 0;
}
static texture original(void){texture t={TEX_VALID,(void *)0x1000,5,1,0,{9,5,0x12345678}};return t;}
int main(void){
 texture t=original(),before=t;
 inject=0;assert(!attempt(&t,5,0));assert(!memcmp(&t,&before,sizeof(t)));
 inject=1;
#ifdef VGL_ETC1_OOM_FALLBACK
 assert(!attempt(&t,5,0));assert(t.data==before.data && t.mip_count==5 && t.gxm_tex.count==5 && t.gxm_tex.untouched==0x12345678 && t.compressed_mip_failed && fallback_records==1);
 t=original();t.use_mips=0;assert(gpu_keep_etc_prefix(&t,5,9));assert(t.gxm_tex.count==1);
#else
 assert(attempt(&t,5,0)==GL_OUT_OF_MEMORY);assert(!memcmp(&t,&before,sizeof(t)) && !fallback_records);
 assert(!gpu_keep_etc_prefix(&t,5,9));
#endif
 t=original();assert(attempt(&t,5,1)==GL_OUT_OF_MEMORY);assert(!t.compressed_mip_failed);
 t=original();assert(attempt(&t,0,0)==GL_OUT_OF_MEMORY);assert(!t.compressed_mip_failed);
 t=original();assert(!gpu_keep_etc_prefix(&t,4,9));
 t.status=0;assert(!gpu_keep_etc_prefix(&t,5,9));
 t=original();t.data=NULL;assert(!gpu_keep_etc_prefix(&t,5,9));
 t=original();t.mip_count=0;assert(!gpu_keep_etc_prefix(&t,5,9));
 t=original();assert(!gpu_keep_etc_prefix(&t,5,8));
 t.gxm_tex.format=8;assert(!gpu_keep_etc_prefix(&t,5,9));
 assert(errors==3);
}
'''
p=Path('work/tests/etc_fallback.c');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(code)
for flags in ([],['-DVGL_ETC1_OOM_FALLBACK']):
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-address','-DVGL_UPLOAD_STAGES',*flags,str(p),'-o','work/tests/etc_fallback'],check=True)
    subprocess.run(['work/tests/etc_fallback'],check=True)
assert 'tex->compressed_mip_failed && mip_level>=tex->mip_count) return;' in s
assert 'if (!mip_level) tex->compressed_mip_failed=0;' in s
print('ETC fallback: actual injected error branch, existing pointer/count preserved, sampler clamp, base/format/data guards and flag-off errors passed')
