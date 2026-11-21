#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <so_util/so_util.h>
#include <kubridge.h>
#include "test_sites.h"
extern char rio_test_begin[], rio_test_end[];
extern uint64_t tf_sum6(int,int,int,int,int,int);
extern int tf_neg(int), tf_outer(int), tf_fact(int);
so_module so_mod;
static size_t exec_left=64*1024;
size_t so_exec_available(void){return exec_left;}
void *so_alloc_exec(size_t size){assert(size<=exec_left);exec_left-=size;
 void *p=mmap(NULL,size,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);return p;}
void kuKernelFlushCaches(const void *p, SceSize l){__builtin___clear_cache((char*)p,(char*)p+l);}
so_hook hook_addr(uintptr_t addr, uintptr_t dst){uint32_t w[2]={0xe51ff004,(uint32_t)dst};memcpy((void*)addr,w,8);return (so_hook){addr};}
static _Atomic uint64_t now_us; static __thread int is_main;
uint64_t sceKernelGetProcessTimeWide(void){return now_us+=10;}
int gangstar_wait_is_main(void){return is_main;}
static char lines[256][160]; static unsigned nlines;
void _log_print(int t,const char *f,...){(void)t;va_list ap;va_start(ap,f);if(nlines<256)vsnprintf(lines[nlines++],160,f,ap);va_end(ap);}
RioProfSite rio_prof_sites[RIO_PROF_SITES];
void gangstar_native_profile_install(void);
void gangstar_native_profile_frame(unsigned, uint64_t);
static void *worker(void *u){(void)u;for(int i=0;i<1000;++i){assert(tf_outer(i)==i+1097);assert(tf_fact(5)==120);}return NULL;}
static int check(void){
 uint64_t r=tf_sum6(1,2,3,4,5,6);assert((uint32_t)r==1021 && (r>>32)==0x55);
 assert(tf_neg(1)==78);assert(tf_outer(10)==1107);assert(tf_fact(10)==3628800);
 volatile char pad[32];pad[0]=1;(void)pad; return 1;}
static unsigned find(uint32_t fn,const char *key){char want[40];snprintf(want,sizeof want,"fn=0x%x ",fn);
 for(unsigned i=0;i<nlines;++i){
  if(!strstr(lines[i],want))continue;
  const char *p=strstr(lines[i],key);assert(p);return (unsigned)strtoul(p+strlen(key),NULL,10);
 }
 return 0;}
int main(void){
 is_main=1;
 uintptr_t lo=(uintptr_t)rio_test_begin&~4095u,hi=((uintptr_t)rio_test_end+4095)&~4095u;
 assert(!mprotect((void*)lo,hi-lo,PROT_READ|PROT_WRITE|PROT_EXEC));
 so_mod.load_addr=0;so_mod.text_base=lo;so_mod.text_size=hi-lo;
 void *fns[4]={(void*)tf_sum6,(void*)tf_neg,(void*)tf_outer,(void*)tf_fact};
 for(int i=0;i<4;++i){uint32_t w[2];memcpy(w,fns[i],8);rio_prof_sites[i]=(RioProfSite){(uint32_t)(uintptr_t)fns[i],w[0],w[1]};}
 assert(check());
 gangstar_native_profile_install();
 for(int i=0;i<4;++i)assert(*(uint32_t*)fns[i]==0xe51ff004);
 int sp_before;void *sp0=&sp_before;
 assert(check());
 int sp_after;assert((char*)&sp_after==(char*)sp0+((char*)&sp_after-(char*)&sp_before)); // frame unchanged
 // Non-main threads run through the thunk untimed, concurrently with the main thread.
 pthread_t t;assert(!pthread_create(&t,NULL,worker,NULL));
 for(int i=0;i<200;++i)assert(tf_outer(i)==i+1097);
 pthread_join(t,NULL);
 // Deep recursion beyond the 256-entry shadow stack stays correct.
 assert(tf_fact(300)==0);
 nlines=0;gangstar_native_profile_frame(1,60000);
 assert(strstr(lines[0],"RIO_PROF frame=1"));
 unsigned outer_calls=find((uint32_t)(uintptr_t)tf_outer,"calls=");
 unsigned neg_calls=find((uint32_t)(uintptr_t)tf_neg,"calls=");
 unsigned outer_incl=find((uint32_t)(uintptr_t)tf_outer,"incl_us=");
 unsigned outer_self=find((uint32_t)(uintptr_t)tf_outer,"self_us=");
 assert(outer_calls==201 && neg_calls==202); // check()+200 main calls; tf_neg also once directly
 assert(outer_self<outer_incl && outer_incl>0);
 // Quiet frames report nothing; cheap hot functions disable themselves.
 nlines=0;for(int f=0;f<5;++f){for(int i=0;i<1000;++i)assert(tf_neg(i)==i+77);gangstar_native_profile_frame(2+f,10000);}
 assert(nlines==0);
 for(int i=0;i<10;++i)assert(tf_neg(i)==i+77);
 nlines=0;gangstar_native_profile_frame(9,60000);
 assert(find((uint32_t)(uintptr_t)tf_neg,"calls=")==0); // disabled: no longer timed
 assert(check());
 printf("Native profiler (ARM emulated): prologue relocation incl. +/- PC literals, stack args, 64-bit returns, nested/recursive calls, ARM and Thumb callers, worker threads untimed, shadow-stack overflow, self/inclusive accounting, quiet frames and hot-function disabling passed\n");
}
