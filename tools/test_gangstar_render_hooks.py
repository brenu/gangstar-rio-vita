from pathlib import Path
import subprocess
src=Path('source/gangstar_render_sample.c').read_text()
src='\n'.join(l for l in src.splitlines() if not l.startswith('#include'))
gl=Path('source/utils/glutil.c').read_text();a=gl.index('#else\nGLint glGetUniformLocation_soloader');b=gl.index('\n#endif\n\n',a)
forward=gl[a+len('#else\n'):b]
s=r'''
#include "gangstar_render_sample.h"
#include <assert.h>
#include <stdarg.h>
static uint64_t clock_value=100;
static uint64_t sceKernelGetProcessTimeWide(void){return ++clock_value;}
static int gangstar_wait_is_main(void){return 1;}
static void sink(const char *format,...){(void)format;}
#define l_info(...) sink(__VA_ARGS__)
'''+src+r'''
typedef unsigned GLuint,GLenum;typedef int GLint,GLsizei;typedef char GLchar;typedef float GLfloat;typedef unsigned char GLboolean;
static unsigned native_calls;
#ifdef GANGSTAR_DIRECT_DRAW_SYNC
static unsigned finishes;
static void glFinish(void){assert(native_calls==5+finishes);++finishes;}
#endif
static GLint glGetUniformLocation(GLuint p,const GLchar *n){assert(p==7 && n[0]=='x');++native_calls;return 77;}
static void glUniformMatrix4fv(GLint l,GLsizei c,GLboolean t,const GLfloat *v){assert(l==2 && c==1 && t==0 && *v==3);++native_calls;}
static void glUniform4fv(GLint l,GLsizei c,const GLfloat *v){assert(l==2 && c==1 && *v==3);++native_calls;}
static void glUniform1i(GLint l,GLint v){assert(l==2 && v==3);++native_calls;}
static void glDrawArrays(GLenum m,GLint f,GLsizei c){assert(m==4 && f==2 && c==3);++native_calls;}
static void glDrawElements(GLenum m,GLsizei c,GLenum t,const void *p){assert(m==4 && c==3 && t==5 && p==(void *)9);++native_calls;}
'''+forward+r'''
int main(void){
 for(unsigned i=0;i<1000;++i){
  uint64_t began=gangstar_render_begin(RIO_RENDER_ELEMENTS,3),stage=gangstar_render_stage_clock();
  assert((began!=0)==(stage!=0));gangstar_render_stage_end(1,stage);
  gangstar_render_end(RIO_RENDER_ELEMENTS,began);assert(!gangstar_render_stage_clock());
 }
 assert(stages[1].calls>0 && stages[1].calls==sample[RIO_RENDER_ELEMENTS].samples);
 gangstar_render_report();assert(!stages[1].calls && !sample[RIO_RENDER_ELEMENTS].calls);
 GLfloat value=3;
 assert(glGetUniformLocation_soloader(7,"x")==77);
 glUniformMatrix4fv_soloader(2,1,0,&value);glUniform4fv_soloader(2,1,&value);glUniform1i_soloader(2,3);
 glDrawArrays_soloader(4,2,3);glDrawElements_soloader(4,3,5,(void *)9);assert(native_calls==6);
#ifdef GANGSTAR_DIRECT_DRAW_SYNC
 assert(finishes==2 && sample[RIO_RENDER_SYNC].calls==2);
#endif
}
'''
p=Path('work/tests/render_hooks.c');p.write_text(s)
for sync in (False,True):
    cmd=['cc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-DGANGSTAR_NATIVE_STARTUP_PROFILE','-DGANGSTAR_DETAILED_RENDER_PROFILE','-I','source']
    if sync:cmd+=['-DGANGSTAR_DIRECT_DRAW_SYNC']
    subprocess.run(cmd+[str(p),'-o','work/tests/render_hooks'],check=True)
    subprocess.run(['work/tests/render_hooks'],check=True)
print('Render hooks: actual forwarding, sampled stages, report reset and completion waits OFF/ON after each draw passed')
