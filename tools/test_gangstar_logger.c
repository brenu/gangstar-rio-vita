#include "utils/logger.h"
#include "utils/log_queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
static pthread_mutex_t mock_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mock_changed=PTHREAD_COND_INITIALIZER;
static int blocked,entered,writes,fail_write;
static char output[512*1024]; static size_t used;
static pthread_t main_thread;
int sceKernelChangeThreadPriority(int a,int b) {(void)a;(void)b;return 0;}
uint64_t sceKernelGetProcessTimeWide(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;
}
int sceClibPrintf(const char *fmt,...) {(void)fmt;return 0;}
int sceClibSnprintf(char *out,size_t n,const char *fmt,...) {
    va_list ap;va_start(ap,fmt);int r=vsnprintf(out,n,fmt,ap);va_end(ap);return r;
}
int sceClibVsnprintf(char *out,size_t n,const char *fmt,va_list ap) {return vsnprintf(out,n,fmt,ap);}
int sceIoOpen(const char *name,int flags,int mode) {(void)name;(void)flags;(void)mode;used=0;writes=entered=0;return 1;}
int sceIoWrite(int fd,const void *data,size_t n) {
    assert(fd==1 && !pthread_equal(main_thread,pthread_self()));
    pthread_mutex_lock(&mock_lock);++writes;entered=1;pthread_cond_broadcast(&mock_changed);
    while(blocked)pthread_cond_wait(&mock_changed,&mock_lock);
    if(fail_write){pthread_mutex_unlock(&mock_lock);return -1;}
    if(n>37)n=37;
    assert(used+n<sizeof(output));memcpy(output+used,data,n);used+=n;output[used]=0;
    pthread_mutex_unlock(&mock_lock);return (int)n;
}
int sceIoSyncByFd(int fd,int flags){(void)flags;assert(fd==1);return 0;}
int sceIoClose(int fd){assert(fd==1);return 0;}
static void *produce(void *arg) {
    unsigned id=(unsigned)(uintptr_t)arg;
    for(unsigned i=0;i<100;++i) l_info("record-%u-%u complete",id,i);
    return NULL;
}
int main(void) {
    main_thread=pthread_self();
    static RioLogQueue queue;
    char *input=malloc(RIO_LOG_CAPACITY),*copy=malloc(RIO_LOG_CAPACITY);assert(input&&copy);
    for(unsigned i=0;i<RIO_LOG_CAPACITY;++i)input[i]=(char)(i%127);
    assert(rio_log_push(&queue,input,RIO_LOG_CAPACITY));
    assert(!rio_log_push(&queue,"full record",11) && queue.dropped==1);
    assert(rio_log_pop(&queue,copy,12345)==12345 && !memcmp(input,copy,12345));
    assert(rio_log_push(&queue,input,12345));
    assert(rio_log_pop(&queue,copy,RIO_LOG_CAPACITY)==RIO_LOG_CAPACITY);
    assert(!memcmp(copy,input+12345,RIO_LOG_CAPACITY-12345));
    assert(!memcmp(copy+RIO_LOG_CAPACITY-12345,input,12345));
    assert(!queue.used && queue.peak==RIO_LOG_CAPACITY);free(input);free(copy);
    blocked=1;gangstar_log_start();l_info("first record");
    pthread_mutex_lock(&mock_lock);while(!entered)pthread_cond_wait(&mock_changed,&mock_lock);pthread_mutex_unlock(&mock_lock);
    pthread_t producers[4];
    for(unsigned i=0;i<4;++i)assert(!pthread_create(&producers[i],NULL,produce,(void *)(uintptr_t)i));
    for(unsigned i=0;i<4;++i)pthread_join(producers[i],NULL);
    // All producers finished while the sole writer is still blocked in I/O.
    char bulk[1001];memset(bulk,'x',1000);bulk[1000]=0;
    for(unsigned i=0;i<1000;++i)l_info("%s",bulk);
    pthread_mutex_lock(&mock_lock);assert(writes==1);blocked=0;pthread_cond_broadcast(&mock_changed);pthread_mutex_unlock(&mock_lock);
    l_fatal("drain full queue");gangstar_log_report();gangstar_log_finish();
    for(unsigned id=0;id<4;++id)for(unsigned i=0;i<100;++i){
        char record[80];snprintf(record,sizeof(record),"record-%u-%u complete\n",id,i);assert(strstr(output,record));
    }
    assert(strstr(output,"RIO_LOG async=1"));
    char *drops=strstr(output,"dropped=");assert(drops && strtoull(drops+8,NULL,10)>0);
    assert(strstr(output,"drain full queue"));
    assert(writes>10);gangstar_log_finish();
    gangstar_log_start();l_fatal("fatal evidence");assert(strstr(output,"fatal evidence"));gangstar_log_finish();
    gangstar_log_start();fail_write=1;l_fatal("failed output must not hang");fail_write=0;
    gangstar_log_report();gangstar_log_finish();assert(strstr(output,"errors=1"));
    puts("Async logger: bounded whole records, wrap/drop accounting, blocked storage with concurrent producers, partial writes, fatal drain and restart passed");
}
