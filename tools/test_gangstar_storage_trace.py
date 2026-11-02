"""Exercise actual storage tracing with controlled wall/CPU clocks and overlap."""
from pathlib import Path
import tempfile,subprocess
with tempfile.TemporaryDirectory(prefix='rio-storage-') as temp:
 d=Path(temp);headers=d/'psp2/kernel';headers.mkdir(parents=True)
 (headers/'threadmgr.h').write_text('#include <stdint.h>\ntypedef struct {unsigned size;uint64_t runClocks;} SceKernelThreadInfo;\nint sceKernelGetThreadInfo(int,SceKernelThreadInfo *);\nint sceKernelGetThreadId(void);\n')
 (headers/'processmgr.h').write_text('#include <stdint.h>\nuint64_t sceKernelGetProcessTimeWide(void);\n')
 harness=d/'main.c';harness.write_text(r'''
#include "gangstar_storage_trace.h"
#include <psp2/kernel/threadmgr.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
static uint64_t wall,cpu;static int fail;static char output[50000];
uint64_t sceKernelGetProcessTimeWide(void){return wall;}
int sceKernelGetThreadId(void){return 42;}
int sceKernelGetThreadInfo(int id,SceKernelThreadInfo *info){assert(id==42 && info->size==sizeof(*info));info->runClocks=cpu;return fail?-1:0;}
void _log_print(int type,const char *format,...){(void)type;va_list args;va_start(args,format);vsnprintf(output+strlen(output),sizeof(output)-strlen(output),format,args);va_end(args);strcat(output,"\n");}
int main(void){
 errno=123;wall=100;cpu=10;struct RioStorageSample outer=rio_storage_begin();assert(errno==123);
 wall=140;rio_storage_locked(&outer);
 rio_storage_write(1);wall=150;struct RioStorageSample inner=rio_storage_begin();
 wall=160;rio_storage_end(inner,1,50,100,100);rio_storage_write(0);
 wall=50100;cpu=70;rio_storage_end(outer,0,1000,131072,131072);assert(errno==123);
 rio_storage_report();assert(errno==123);
 assert(strstr(output,"wall_us=50000 cpu_us=60 cpu_valid=1 lock_wait_us=40 io_interval_us=49960 log_write_edges=2 writer_at_begin=0 other_reads=1"));
 output[0]=0;fail=1;outer=rio_storage_begin();wall+=30000;rio_storage_end(outer,2,1,1,-1);rio_storage_report();assert(strstr(output,"cpu_valid=0"));
 output[0]=0;fail=0;
 for(unsigned i=0;i<70;++i){outer=rio_storage_begin();wall+=30000;rio_storage_end(outer,0,1,1,1);}
 rio_storage_report();assert(strstr(output,"dropped=6"));output[0]=0;rio_storage_report();assert(!*output);
}
''')
 exe=d/'test'
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-DGANGSTAR_STORAGE_TRACE','-DDEBUG_SOLOADER','-I'+str(d),'-Isource','source/gangstar_storage_trace.c',str(harness),'-pthread','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Storage trace: actual lock/syscall wall intervals, CPU validity, read/write overlap, fast-read omission, bounded queue, reset and errno preservation passed')
