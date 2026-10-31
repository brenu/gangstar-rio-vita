"""Verify real diagnostic wrappers forward uniforms without idle table work."""
from pathlib import Path
import subprocess
s=Path('source/utils/glutil.c').read_text();a=s.index('static void hud_values(');b=s.index('extern void vglDebugProgramState',a)
prefix=r"""
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "gangstar_render_sample.h"
static unsigned timed_begin,timed_end;
static uint64_t gangstar_test_began;
uint64_t gangstar_render_begin(unsigned kind,unsigned work){assert(kind>=RIO_RENDER_MATRIX && kind<=RIO_RENDER_UNIFORM1);(void)work;++timed_begin;return ++gangstar_test_began;}
void gangstar_render_end(unsigned kind,uint64_t began){assert(kind>=RIO_RENDER_MATRIX && kind<=RIO_RENDER_UNIFORM1 && began==gangstar_test_began);++timed_end;}
typedef int GLint;typedef int GLsizei;typedef unsigned char GLboolean;typedef float GLfloat;
struct HudUniform {int count,transpose,values;float value[16];};
static struct HudUniform entry;static void *hud_trace;static unsigned queries,matrices,vectors,integers;
static const void *forwarded;static int forwarded_location,forwarded_count,forwarded_transpose,forwarded_integer;
static struct HudUniform *hud_uniform(GLint l,int create){(void)l;assert(!create);++queries;return &entry;}
static void glUniformMatrix4fv(GLint l,GLsizei n,GLboolean t,const GLfloat *v){++matrices;forwarded=v;forwarded_location=l;forwarded_count=n;forwarded_transpose=t;}
static void glUniform4fv(GLint l,GLsizei n,const GLfloat *v){++vectors;forwarded=v;forwarded_location=l;forwarded_count=n;}
static void glUniform1i(GLint l,GLint v){++integers;forwarded_location=l;forwarded_integer=v;}
"""
main=r"""
int main(void){
 float idle[32]={0};
 for(unsigned i=0;i<10000;++i){
  glUniformMatrix4fv_soloader(7,2,1,idle);
  assert(forwarded_location==7 && forwarded_count==2 && forwarded_transpose==1 && forwarded==idle);
  glUniform4fv_soloader(9,3,(idle+4));
  assert(forwarded_location==9 && forwarded_count==3 && forwarded==(idle+4));
  glUniform1i_soloader(11,42);assert(forwarded_location==11 && forwarded_integer==42);
 }
 assert(!queries && matrices==10000 && vectors==10000 && integers==10000);
 hud_trace=&entry;float values[16];for(unsigned i=0;i<16;++i)values[i]=(float)i;
 glUniformMatrix4fv_soloader(7,1,0,values);assert(queries==1 && entry.values==16 && !memcmp(entry.value,values,sizeof(values)));
 glUniform4fv_soloader(9,1,values+4);assert(queries==2 && entry.values==4 && !memcmp(entry.value,values+4,4*sizeof(float)));
 glUniform1i_soloader(11,13);assert(queries==3 && entry.values==1 && entry.value[0]==13);
 hud_trace=0;glUniform1i_soloader(11,14);assert(queries==3 && forwarded_integer==14);
 assert(timed_begin==30004 && timed_end==30004);
}
"""
p=Path('work/tests/trace_idle.c');p.write_text(prefix+s[a:b]+main)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I','source',str(p),'-o','work/tests/trace_idle'],check=True)
subprocess.run(['work/tests/trace_idle'],check=True)
print('Trace idle: actual wrappers forward 30000 idle uniform calls with zero diagnostic lookups, record active captures, and stop recording after capture')
