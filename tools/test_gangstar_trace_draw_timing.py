"""Compile real trace-mode draw wrappers; verify timing and draw forwarding."""
from pathlib import Path
import subprocess
s=Path('source/utils/glutil.c').read_text()
a=s.index('void glDrawArrays_soloader(')
b=s.index('\n#else\nGLint glGetUniformLocation_soloader',a)
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include "gangstar_render_sample.h"
typedef unsigned GLenum;typedef int GLint;typedef int GLsizei;
#define GL_CURRENT_PROGRAM 1
static void *hud_trace;static unsigned begins,ends,draws,queries,kind_seen,work_seen;
static unsigned mode_seen,type_seen;static int first_seen,count_seen;static const void *indices_seen;
static uint64_t stamp;static int sampling;
static void hud_trace_draw(GLenum m,GLint f,GLsizei n,GLenum t,const void *p){(void)m;(void)f;(void)n;(void)t;(void)p;}
uint64_t gangstar_render_begin(unsigned k,unsigned w){++begins;kind_seen=k;work_seen=w;return sampling?++stamp:0;}
void gangstar_render_end(unsigned k,uint64_t t){assert(k==kind_seen && t==(sampling?stamp:0));++ends;}
static void glDrawArrays(GLenum m,GLint f,GLsizei n){assert(begins==ends+1);++draws;mode_seen=m;first_seen=f;count_seen=n;}
static void glDrawElements(GLenum m,GLsizei n,GLenum t,const void *p){assert(begins==ends+1);++draws;mode_seen=m;count_seen=n;type_seen=t;indices_seen=p;}
static void glGetIntegerv(GLenum k,GLint *v){assert(k==GL_CURRENT_PROGRAM);++queries;*v=7;}
static void vglSetDebugDrawOutput(void *p){assert(!p || p==hud_trace);}
'''
main=r'''
int main(void){uint16_t indices[]={1,4,2};
 for(sampling=0;sampling<2;++sampling)for(unsigned trace=0;trace<2;++trace){
  hud_trace=trace?indices:NULL;
  glDrawArrays_soloader(4,3,6);
  assert(kind_seen==RIO_RENDER_ARRAYS && work_seen==6 && mode_seen==4 && first_seen==3 && count_seen==6);
  glDrawElements_soloader(5,3,0x1403,indices);
  assert(kind_seen==RIO_RENDER_ELEMENTS && work_seen==3 && mode_seen==5 && count_seen==3 && type_seen==0x1403 && indices_seen==indices);
 }
 assert(begins==8 && ends==8 && draws==8 && queries==2);
 sampling=0;hud_trace=NULL;
 glDrawArrays_soloader(4,-1,-5);assert(work_seen==0 && first_seen==-1 && count_seen==-5);
 glDrawElements_soloader(4,-2,0x1403,NULL);assert(work_seen==0 && count_seen==-2 && !indices_seen);
}
'''
out=Path('work/tests/trace_draw_timing.c');out.write_text(prefix+s[a:b]+main)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I','source',str(out),'-o','work/tests/trace_draw_timing'],check=True)
subprocess.run(['work/tests/trace_draw_timing'],check=True)
print('Trace draw timing: real trace wrappers time sampled/unsampled draws, preserve arguments, and retain on-demand capture')
