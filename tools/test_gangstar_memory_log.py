"""Exercise the real async logger with bounded RAM buffering and explicit export."""
from pathlib import Path
import subprocess,tempfile
with tempfile.TemporaryDirectory(prefix='rio-ram-log-') as temp:
 d=Path(temp);h=d/'main.c';h.write_text(r'''
#include "utils/logger.h"
#include "utils/log_queue.h"
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
static char output[3*1024*1024];static size_t used;static atomic_uint writes;static int ack;
uint64_t sceKernelGetProcessTimeWide(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
int sceKernelChangeThreadPriority(int a,int b){(void)a;(void)b;return 0;}
int sceClibPrintf(const char *format,...){
 if(!strcmp(format,"%.*s")){va_list a;va_start(a,format);int n=va_arg(a,int);va_end(a);assert(n<=(int)RIO_LOG_CHUNK);}
 return 0;
}
int sceClibSnprintf(char *out,size_t n,const char *fmt,...){va_list ap;va_start(ap,fmt);int r=vsnprintf(out,n,fmt,ap);va_end(ap);return r;}
int sceClibVsnprintf(char *out,size_t n,const char *fmt,va_list ap){return vsnprintf(out,n,fmt,ap);}
int sceIoOpen(const char *path,int flags,int mode){(void)flags;(void)mode;if(strstr(path,"memory-log.ready")){ack=open(path,O_WRONLY|O_CREAT|O_TRUNC,0600);assert(ack>=0);return 2;}used=0;atomic_store(&writes,0);return 1;}
int sceIoWrite(int fd,const void *data,size_t n){
 if(fd==2){if(n>5)n=5;return write(ack,data,n);}
 assert(fd==1);atomic_fetch_add(&writes,1);if(n>4096)n=4096;
 assert(used+n<sizeof(output));memcpy(output+used,data,n);used+=n;output[used]=0;return (int)n;
}
int sceIoSyncByFd(int fd,int flags){(void)fd;(void)flags;return 0;}
int sceIoClose(int fd){if(fd==2)close(ack);return 0;}
static void *producer(void *v){(void)v;for(unsigned i=0;i<100;++i)l_info("thread record %u",i);return NULL;}
#include "utils/logger.c"
static void await_export(void){
 struct timespec pause={0,1000000};
 for(unsigned i=0;i<5000;++i){
  FILE *f=fopen(DATA_PATH "memory-log.ready","r");
  if(f){char status[128]={0};char *line=fgets(status,sizeof(status),f);fclose(f);if(line && strchr(status,'\n'))return;}
  nanosleep(&pause,NULL);
 }
 assert(!"export request lost before queue drained");
}
int main(void){
 gangstar_log_start();l_info("first RAM record");l_error("RAM error");
 pthread_t threads[4];for(unsigned i=0;i<4;++i)assert(!pthread_create(&threads[i],NULL,producer,NULL));
 for(unsigned i=0;i<4;++i)pthread_join(threads[i],NULL);
 l_fatal("drain queue to RAM");assert(!atomic_load(&writes));
 char bulk[1001];memset(bulk,'x',1000);bulk[1000]=0;
 for(unsigned i=0;i<2300;++i)l_fatal("%s",bulk);
 assert(!atomic_load(&writes));
 int trigger=open(DATA_PATH "memory-log.flush",O_WRONLY|O_CREAT,0600);assert(trigger>=0);close(trigger);
 gangstar_log_report();await_export();
 assert(atomic_load(&writes)>0 && strstr(output,"first RAM record") && strstr(output,"RAM error"));
 FILE *f=fopen(DATA_PATH "memory-log.ready","r");assert(f);char status[128]={0};assert(fgets(status,sizeof(status),f));fclose(f);
 assert(strstr(status,"exported=") && strstr(status,"dropped_bytes=") && !strstr(status,"dropped_bytes=0 "));
 gangstar_log_finish();
 gangstar_log_start();l_fatal("second session");assert(!atomic_load(&writes));
 // Deterministically request export with queued data still pending. The
 // worker must retain the request across the pop instead of discarding it.
 pthread_mutex_lock(&log_lock);
 assert(rio_log_push(&log_queue,"busy-queue-export\n",18));
 pthread_cond_signal(&log_changed);pthread_mutex_unlock(&log_lock);
 trigger=open(DATA_PATH "memory-log.flush",O_WRONLY|O_CREAT,0600);assert(trigger>=0);close(trigger);
 gangstar_log_report();await_export();
 unsigned before=atomic_load(&writes);l_fatal("still RAM after export");assert(atomic_load(&writes)==before);
 gangstar_log_finish();
 assert(strstr(output,"second session") && strstr(output,"busy-queue-export"));
 gangstar_log_start();l_fatal("finish-only session");assert(!atomic_load(&writes));gangstar_log_finish();
 assert(strstr(output,"finish-only session"));
}
''')
 exe=d/'test'
 subprocess.run(['cc','-std=c11','-O2','-D_POSIX_C_SOURCE=200809L','-DDEBUG_SOLOADER','-DGANGSTAR_ASYNC_LOG','-DGANGSTAR_MEMORY_LOG',f'-DDATA_PATH="{d}/"','-Wall','-Wextra','-Werror','-Itools/logger_mocks','-Isource','-pthread',str(h),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=30)
print('RAM logger: zero pre-export writes, concurrent producers, bounded overflow, error/fatal buffering, partial-write export acknowledgement, finish drain and session reset passed')
