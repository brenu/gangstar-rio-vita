from pathlib import Path
import struct,subprocess,tempfile
elf=Path('../work/libgl2jni.so').read_bytes()
assert struct.unpack_from('<II',elf,0x81dc94)==(0xe92d4010,0xe5903030)
assert struct.unpack_from('<I',elf,0x467ae8)==(0xe1a07002,) # filename preserved from r2
assert struct.unpack_from('<I',elf,0x467b10)==(0xe1a01007,) # filename passed to strcpy
s=Path('source/gangstar_native_startup.c').read_text()
a=s.index('#define PROBE(N)');b=s.index('PROBE(0)',a);generic=s[a:b]
a=s.index('#ifdef GANGSTAR_STORAGE_TRACE\n// Main-thread scope only.')
b=s.index('// Generic probes use',a)
context=s[a:b]
a=s.index('static uintptr_t task_read_run(');b=s.index('#ifdef GANGSTAR_MATERIAL_PREFETCH\nstatic uintptr_t material_handler_vtable',a)
wrapper=s[a:b]
with tempfile.TemporaryDirectory(prefix='rio-task-read-') as temp:
 p=Path(temp);c=p/'test.c'
 c.write_text(r'''
#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
static int main_thread=1,logs;
static int gangstar_wait_is_main(void){return main_thread;}
#define l_info(...) (++logs)
''' +context+r'''
typedef uintptr_t (*NativeCall)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
struct {NativeCall original;uint64_t us;unsigned calls;const char *label;} probes[60];
#define PHASE_BASE 0
''' + wrapper+r'''
#define rio_audio_scope_active() 0
#define rio_diagnostic_clock() 0
#define rio_audio_phase(...) ((void)0)
#define gangstar_wait_begin() 1
#define RIO_WAIT_NATIVE 0
#define RIO_AUDIO_FILE 0
static uint64_t gangstar_wait_end(int t,uint64_t start,uintptr_t caller,uintptr_t obj,unsigned requested,int result,const char *label){(void)t;(void)start;(void)caller;(void)obj;(void)requested;(void)result;(void)label;return 50000;}
''' +generic+r'''
PROBE(13)
static uintptr_t file_open(uintptr_t out,uintptr_t filesystem,uintptr_t name,uintptr_t unused){assert(out==123 && !strcmp((char *)filesystem,"filesystem") && !strcmp((char *)name,"middleclass_01_lightmap00.tga") && unused==0);return 55;}

static uintptr_t inner,outer;static int depth,calls;
static uintptr_t native(uintptr_t task,uintptr_t b,uintptr_t c,uintptr_t d){
 assert(b==2 && c==3 && d==4);++calls;
 if(!main_thread){assert(task_read_context.task==0);return 77;}
 assert(task_read_context.task==task && task_read_context.vtable==0x1110 && task_read_context.function_vtable==0x2220);
 if(!depth){depth=1;assert(task_read_run(inner,2,3,4)==77);assert(task_read_context.task==outer);}
 return 77;
}
int main(void){
 probes[13].original=file_open;assert(probe_13(123,(uintptr_t)"filesystem",(uintptr_t)"middleclass_01_lightmap00.tga",0)==55);assert(logs==1);logs=0;task_read_trace_count=0;
 void *m=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);assert(m!=MAP_FAILED);
 outer=(uintptr_t)m;inner=outer+128;uint32_t vt=0x1110,f=(uint32_t)(outer+256),fvt=0x2220;
 memcpy((void *)outer,&vt,4);memcpy((void *)inner,&vt,4);memcpy((void *)(outer+0x30),&f,4);memcpy((void *)(inner+0x30),&f,4);memcpy((void *)(uintptr_t)f,&fvt,4);
 probes[59].original=native;assert(task_read_run(outer,2,3,4)==77 && calls==2 && !task_read_context.task);
 main_thread=0;assert(task_read_run(outer,2,3,4)==77 && calls==3);main_thread=1;
 task_read_trace(14,"middleclass_01_lightmap00.tga",50000);task_read_trace(13,"middleclass_01_lightmap00.tga",10000);task_read_trace(13,"other_lightmap.tga",50000);assert(!logs);
 for(unsigned i=0;i<100;++i)task_read_trace(13,"_packetc/middleclass_01_lightmap00.tga",50000);
 assert(logs==32 && task_read_trace_count==32);assert(!munmap(m,4096));return 0;
}
''')
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-DGANGSTAR_STORAGE_TRACE',str(c),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Task-read trace: native signature, nested context restoration, worker exclusion, unchanged arguments/return and bounded slow-target logging passed')
