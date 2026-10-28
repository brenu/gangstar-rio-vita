#include "gangstar_wait_perf.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static uint64_t now = 1, largest;
static int thread = 1;
static unsigned details;
uint64_t sceKernelGetProcessTimeWide(void) { return now; }
int sceKernelGetThreadId(void) { return thread; }
void _log_print(int type, const char *format, ...) {
    (void)type;
    if (!strstr(format, "RIO_WAIT_DETAIL")) return;
    va_list args; va_start(args, format);
    (void)va_arg(args, unsigned); (void)va_arg(args, const char *);
    uint64_t elapsed = va_arg(args, unsigned long long);
    if (elapsed > largest) largest = elapsed;
    ++details; va_end(args);
}
int main(void) {
    gangstar_wait_frame();
    uint64_t began = gangstar_wait_begin(); now += 5000;
    errno = EINTR;
    gangstar_wait_end(RIO_WAIT_SLEEP, began, 123, 0, 5000, -1, "sleep");
    assert(errno == EINTR);
    GangstarWaitPerf stats = gangstar_wait_stats();
    assert(stats.calls[RIO_WAIT_SLEEP] == 1 && stats.us[RIO_WAIT_SLEEP] == 5000);
    thread = 2; assert(!gangstar_wait_begin());
    gangstar_wait_end(RIO_WAIT_MUTEX, 0, 0, 0, 0, 0, "mutex");
    stats = gangstar_wait_stats(); assert(!stats.calls[RIO_WAIT_MUTEX]);
    thread = 1; gangstar_wait_frame();
    for (unsigned i = 0; i < 40; ++i) {
        began = gangstar_wait_begin(); now += 1000;
        gangstar_wait_end(RIO_WAIT_FILE, began, i, 0, 0, 0, "file");
    }
    began = gangstar_wait_begin(); now += 30000000;
    gangstar_wait_end(RIO_WAIT_COND, began, 456, 789, 0, 0, "condition");
    gangstar_wait_report(17);
    assert(details == 32 && largest == 30000000); // Retain late long waits.
    stats = gangstar_wait_stats();
    assert(stats.calls[RIO_WAIT_FILE] == 40 && stats.us[RIO_WAIT_FILE] == 40000);
    assert(stats.us[RIO_WAIT_COND] == 30000000);
    GangstarWaitEvent event=gangstar_wait_longest(RIO_WAIT_COND);
    assert(event.us==30000000 && event.caller==456 && event.object==789 && !strcmp(event.operation,"condition"));
    GangstarConditionTrace trace={.setup_us=2,.before_signal_us=21000000,.resume_us=8000000,.cleanup_us=3,.relock_us=4,.signal_thread=42,.signaled=1};
    gangstar_wait_condition(789,30000000,&trace);
    event=gangstar_wait_longest(RIO_WAIT_COND);
    assert(event.condition.resume_us==8000000 && event.condition.signal_thread==42 && event.condition.signaled);
    trace.resume_us=99;
    gangstar_wait_condition(789,100,&trace);
    gangstar_wait_condition(12,30000000,&trace);
    thread=2;gangstar_wait_condition(789,30000000,&trace);thread=1;
    assert(gangstar_wait_longest(RIO_WAIT_COND).condition.resume_us==8000000);
    assert(!gangstar_wait_longest(RIO_WAIT_MUTEX).us);
    assert(gangstar_wait_longest(RIO_WAIT_FILE).us==1000);
    errno=EINTR; (void)gangstar_wait_longest(RIO_WAIT_FILE); assert(errno==EINTR);
    gangstar_wait_frame(); stats = gangstar_wait_stats(); assert(!stats.us[RIO_WAIT_COND]); assert(!gangstar_wait_longest(RIO_WAIT_COND).us);
    char path[256];memset(path,'x',sizeof(path));path[sizeof(path)-1]=0;
    began=gangstar_wait_begin();now+=6000;errno=ENOENT;
    gangstar_wait_end_context(RIO_WAIT_FILE,began,12,0,0,-1,"fopen",path,0);
    assert(errno==ENOENT);memset(path,'y',sizeof(path));
    event=gangstar_wait_longest(RIO_WAIT_FILE);
    assert(event.saved_errno==ENOENT && strlen(event.path)==191 && event.path[0]=='x');
    began=gangstar_wait_begin();now+=8000;
    gangstar_wait_end_context(RIO_WAIT_MUTEX,began,13,14,0,0,"mutex",NULL,1234);
    gangstar_wait_owner_site(14,0x123456,8000);
    gangstar_wait_owner_site(14,0xbad,9999);
    event=gangstar_wait_longest(RIO_WAIT_MUTEX);
    assert(event.owner_acquire_site==0x123456);
    assert(event.observed_owner==1234 && !event.path[0] && event.saved_errno==0);
    thread=2;assert(!gangstar_wait_begin());
    gangstar_wait_end_context(RIO_WAIT_FILE,0,0,0,0,-1,"fopen",NULL,0);
    assert(gangstar_wait_longest(RIO_WAIT_FILE).us==6000);
    puts("Wait profiler: complete totals, errno preservation, thread isolation, reset and bounded longest-event retention passed");
}
