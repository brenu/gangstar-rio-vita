"""Verify touch filtering calls the original without rewriting executable code."""
from pathlib import Path
import subprocess
s=Path('source/gangstar_controls.c').read_text()
a=s.index('static void physical_button_touch_update(');b=s.index('static void physical_stick_touch_update(',a)
body=s[a:b]
assert 'kuKernelFlushCaches' not in body and 'sceClibMemcpy' not in body
assert 'rio_startup_trampoline(trampoline,button_update,update_expected,literals)' in s
p=Path('work/tests/touch_update.c')
p.write_text(r"""
#include <assert.h>
#include <string.h>
#include "gangstar_button_policy.h"
static unsigned calls;
static void *expected;
static double expected_time;
static void original(void *button,double seconds){assert(button==expected && seconds==expected_time);++calls;}
static void (*touch_button_update_original)(void *,double)=original;
"""+body+r"""
int main(void){
 static unsigned char button[112];expected=button;
 for(unsigned type=0;type<40;++type){
  memcpy(button+0x68,&type,4);expected_time=type*0.125;
  unsigned before=calls;physical_button_touch_update(button,expected_time);
  assert(calls==before+!rio_touch_button_mapped(type));
 }
}
""")
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Isource',str(p),'-o','work/tests/touch_update'],check=True)
subprocess.run(['work/tests/touch_update'],check=True)
print('Touch update: mapped gameplay suppressed, context/UI updates retain pointer and double argument; no runtime code patching or cache flushes')
