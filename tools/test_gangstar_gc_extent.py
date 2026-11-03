"""Compare the actual GC body with full scanning across queue rotations."""
from pathlib import Path
import subprocess,os
s=Path('lib/vitagl/source/gxm.c').read_text()
a=s.index('\t\t// Purging all elements marked for deletion')
b=s.index('\n#ifndef HAVE_SINGLE_THREADED_GC\n\t\tsceKernelSignalSema',a)
body=s[a:b]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "retire_queue.h"
#define FRAME_PURGE_FREQ 4
#define FRAME_PURGE_LIST_SIZE 8
#define FRAME_PURGE_RENDERTARGETS_LIST_SIZE 2
#define MIN(a,b) ((a)<(b)?(a):(b))
static void *frame_purge_list[4][8],*frame_rt_purge_list[4][2],*ref[4][8],*ref_rt[4][2];
static int frame_purge_idx,frame_elem_purge_idx,frame_rt_purge_idx,frame_purge_clean_idx=1;
#ifdef HAVE_SINGLE_THREADED_GC
static size_t frame_purge_counts[4];
#endif
static unsigned ref_prod,ref_clean=1;
static uintptr_t actual[16],expected[16],serial;
static unsigned actual_n,expected_n;
static size_t scanned;
static uint64_t gc_cycles,gc_elapsed,gc_worst,gc_released,gc_peak;
static uint64_t sceKernelGetProcessTimeWide(void){static uint64_t t;return ++t;}
static void vgl_free(void *p){assert(actual_n<16);actual[actual_n++]=(uintptr_t)p;}
static void gc_profile_release(void *p){++gc_released;vgl_free(p);}
static void sceGxmDestroyRenderTarget(void *p){vgl_free(p);}
static void counted_collect(void **q,size_t n,void(*f)(void *)){scanned+=n;vgl_retire_queue_collect(q,n,f);}
#define vgl_retire_queue_collect counted_collect
static void collect(void){
''' + body + r'''
}
static void reference(void){
 for(unsigned i=0;i<8;++i){if(ref[ref_clean][i])expected[expected_n++]=(uintptr_t)ref[ref_clean][i];ref[ref_clean][i]=NULL;}
 for(unsigned i=0;i<2;++i){if(ref_rt[ref_clean][i])expected[expected_n++]=(uintptr_t)ref_rt[ref_clean][i];ref_rt[ref_clean][i]=NULL;}
 ref_clean=(ref_clean+1)%4;ref_prod=(ref_prod+1)%4;
}
static void cycle(void){
 actual_n=expected_n=0;collect();reference();
 assert(actual_n==expected_n && !memcmp(actual,expected,actual_n*sizeof(*actual)));
 assert((unsigned)frame_purge_idx==ref_prod && (unsigned)frame_purge_clean_idx==ref_clean);
 assert(!frame_elem_purge_idx && !frame_rt_purge_idx);
 assert(!memcmp(frame_purge_list,ref,sizeof(ref)) && !memcmp(frame_rt_purge_list,ref_rt,sizeof(ref_rt)));
}
int main(void){
 for(unsigned i=0;i<16;++i)cycle();
#ifdef HAVE_SINGLE_THREADED_GC
 assert(!scanned); // Empty queues never touch their allocation slots.
#else
 assert(scanned==16*8);
#endif
 for(unsigned f=0;f<20000;++f){
  unsigned n=(f*17+3)%9;
  for(unsigned i=0;i<n;++i){
   void *p=i%3==0?NULL:(void *)(++serial);
   assert(!frame_purge_list[frame_purge_idx][i]);
   frame_purge_list[frame_purge_idx][frame_elem_purge_idx++]=p;ref[ref_prod][i]=p;
  }
  if(f%17==0){void *p=(void *)(++serial);frame_rt_purge_list[frame_purge_idx][frame_rt_purge_idx++]=p;ref_rt[ref_prod][0]=p;}
  cycle();
  if(f%101==0){
   // Extra allocation-pressure collection calls keep the original schedule.
   for(unsigned j=0;j<6;++j)cycle();
  }
 }
 for(unsigned i=0;i<4;++i)cycle();
 for(unsigned f=0;f<4;++f)for(unsigned i=0;i<8;++i)assert(!frame_purge_list[f][i]);
}
'''
p=Path('work/tests/gc_extent.c');p.parent.mkdir(parents=True,exist_ok=True);p.write_text(code)
for flags in ([],['-DHAVE_SINGLE_THREADED_GC'],['-DHAVE_SINGLE_THREADED_GC','-DVGL_UPLOAD_STAGES','-fsanitize=address,undefined','-fno-omit-frame-pointer']):
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Ilib/vitagl/source/utils',*flags,str(p),'-o','work/tests/gc_extent'],check=True)
    subprocess.run(['work/tests/gc_extent'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
print('GC extent: actual collector matches full scans for 20000 frames, holes/final slot, forced cycles, same release order/delay, zero empty scans and worker fallback (ASan/UBSan)')
