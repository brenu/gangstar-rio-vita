"""Exercise the production read-only queue census and interval counters."""
from pathlib import Path
import subprocess
s=Path('lib/vitagl/source/gxm.c').read_text()
a=s.index('void gangstar_gc_census(');b=s.index('\n#endif',a)
code=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#define FRAME_PURGE_FREQ 4
#define FRAME_PURGE_LIST_SIZE 8
#define FRAME_PURGE_RENDERTARGETS_LIST_SIZE 2
#define VGL_MEM_VRAM 0
#define VGL_MEM_EXTERNAL 4
static void *frame_purge_list[4][8],*frame_rt_purge_list[4][2];
static uint64_t gc_cycles=10,gc_elapsed=123,gc_worst=30,gc_released=7,gc_peak=20;
static unsigned queries;
static int vgl_mem_get_type_by_addr(void *p){return p==(void *)3?-1:(p==(void *)2?4:0);}
static size_t vgl_malloc_usable_size(void *p){assert(p!=(void *)3);++queries;return p==(void *)1?128:512;}
''' + s[a:b] + r'''
int main(void){
 frame_purge_list[0][0]=(void *)1;
 frame_purge_list[2][7]=(void *)2; // A hole must not truncate a queue.
 frame_purge_list[3][3]=(void *)3; // Do not dereference unclassified entries.
 frame_rt_purge_list[1][1]=(void *)4;
 void *before[4][8];memcpy(before,frame_purge_list,sizeof(before));
 uint64_t out[9];gangstar_gc_census(out);
 assert(out[0]==10 && out[1]==123 && out[2]==30 && out[3]==7);
 assert(out[4]==3 && out[5]==640 && out[6]==20 && out[7]==1 && out[8]==1 && queries==2);
 assert(!memcmp(before,frame_purge_list,sizeof(before)) && frame_rt_purge_list[1][1]==(void *)4);
 gangstar_gc_census(out);assert(!out[0] && !out[1] && !out[2] && !out[3] && !out[6]);
 assert(out[4]==3 && out[5]==640 && out[7]==1 && out[8]==1);
}
'''
p=Path('work/tests/gc_census.c');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(code)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p),'-o','work/tests/gc_census'],check=True)
subprocess.run(['work/tests/gc_census'],check=True)
print('GC census: production queue walk, holes, unknown pointers, byte accounting, interval reset and unchanged ownership passed')
