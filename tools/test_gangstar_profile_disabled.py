"""Verify profiling-off preserves functional hooks and does no clock work."""
from pathlib import Path
import subprocess
s=Path('source/gangstar_native_startup.c').read_text();a=s.index('static int native_probe_needed(');b=s.index('// Borrow only while',a)
for base in (47,50):
 for detail in (False,True):
  p=Path('work/tests/profile_hook_gate.c')
  p.write_text('#include <assert.h>\n#define PHASE_BASE %d\n#define RIO_PHASE_NATIVE_TID 46\n#define RIO_PHASE_SUBMIT_BASE 47\n'%base+('#define GANGSTAR_DETAILED_RENDER_PROFILE\n' if detail else '')+s[a:b]+'''\nint main(void){for(unsigned i=0;i<PHASE_BASE+55;++i)assert(native_probe_needed(i)=='''+('1' if detail else '(i<PHASE_BASE || i==PHASE_BASE+46 || i>=PHASE_BASE+47)')+');}\n')
  subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p),'-o','work/tests/profile_hook_gate'],check=True)
  subprocess.run(['work/tests/profile_hook_gate'],check=True)
p=Path('work/tests/profile_disabled.c');p.write_text(r'''
#include "gangstar_render_sample.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
uint64_t gangstar_render_stage_clock(void);
void gangstar_render_stage_end(unsigned,uint64_t);
uint64_t gangstar_vertex_copy_begin(void);
void gangstar_vertex_copy_end(uint64_t);
static unsigned rejected,logs;
void vglGetRejectedDrawSetupStats(unsigned *out){*out=rejected;rejected=0;}
void _log_print(int level,const char *fmt,...){(void)level;(void)fmt;++logs;}
int main(void){
 for(unsigned i=0;i<100000;++i){
  assert(!gangstar_render_begin(RIO_RENDER_ELEMENTS,100));
  gangstar_render_end(RIO_RENDER_ELEMENTS,0);
  assert(!gangstar_render_stage_clock());gangstar_render_stage_end(3,0);
  assert(!gangstar_vertex_copy_begin());gangstar_vertex_copy_end(0);
 }
 gangstar_render_report();assert(!logs);
 rejected=3;gangstar_render_report();assert(logs==1 && !rejected);
}
''')
subprocess.run(['cc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-DDEBUG_SOLOADER','-I','source','source/gangstar_render_sample.c',str(p),'-o','work/tests/profile_disabled'],check=True)
subprocess.run(['work/tests/profile_disabled'],check=True)
print('Profiling disabled: functional hooks/native ID cache retained with both probe layouts; detailed hooks excluded; actual no-clock sampler stubs preserve rejected-draw reporting')
