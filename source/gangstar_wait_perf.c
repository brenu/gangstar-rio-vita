#include "gangstar_wait_perf.h"
#include "utils/logger.h"
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>
#include <errno.h>
#include <string.h>
static atomic_int frame_thread;
static GangstarWaitPerf stats;
static GangstarWaitEvent events[32];
static unsigned event_count;
void gangstar_wait_frame(void) {
    atomic_store_explicit(&frame_thread, sceKernelGetThreadId(), memory_order_relaxed);
    stats = (GangstarWaitPerf){0}; event_count = 0;
}
uint64_t gangstar_wait_begin(void) {
    return atomic_load_explicit(&frame_thread, memory_order_relaxed) == sceKernelGetThreadId()
        ? sceKernelGetProcessTimeWide() : 0;
}
int gangstar_wait_is_main(void) {
    return atomic_load_explicit(&frame_thread, memory_order_relaxed) == sceKernelGetThreadId();
}
uint64_t gangstar_wait_end(unsigned kind, uint64_t began, uintptr_t caller, uintptr_t object,
    uint64_t requested, int result, const char *operation) {
    return gangstar_wait_end_context(kind,began,caller,object,requested,result,operation,NULL,0);
}
uint64_t gangstar_wait_end_context(unsigned kind,uint64_t began,uintptr_t caller,uintptr_t object,
    uint64_t requested,int result,const char *operation,const char *path,int observed_owner) {
    if (!began || kind >= RIO_WAIT_KINDS) return 0;
    int saved_errno = errno;
    uint64_t elapsed = sceKernelGetProcessTimeWide() - began;
    ++stats.calls[kind]; stats.us[kind] += elapsed;
    if (elapsed >= 1000) {
        unsigned slot = event_count;
        if (slot == 32) {
            slot = 0;
            for (unsigned i = 1; i < 32; ++i) if (events[i].us < events[slot].us) slot = i;
            if (events[slot].us >= elapsed) { errno = saved_errno; return elapsed; }
        } else ++event_count;
        events[slot] = (GangstarWaitEvent){.us=elapsed,.requested=requested,.caller=caller,
            .object=object,.result=result,.operation=operation,.kind=kind,
            .saved_errno=result < 0 ? saved_errno : 0,.observed_owner=observed_owner};
        if(path) {
            size_t n=0; while(n<sizeof(events[slot].path)-1 && path[n])++n;
            memcpy(events[slot].path,path,n);events[slot].path[n]=0;
        }
    }
    errno = saved_errno;
    return elapsed;
}
void gangstar_wait_owner_site(uintptr_t object,uintptr_t site,uint64_t elapsed) {
    if(!gangstar_wait_is_main())return;
    for(unsigned i=event_count;i>0;--i)if(events[i-1].kind==RIO_WAIT_MUTEX && events[i-1].object==object && events[i-1].us==elapsed) {
        events[i-1].owner_acquire_site=site;break;
    }
}
void gangstar_wait_condition(uintptr_t object, uint64_t elapsed, const GangstarConditionTrace *trace) {
    if (!trace || !gangstar_wait_is_main()) return;
    for (unsigned i=event_count; i>0; --i)
        if (events[i-1].kind==RIO_WAIT_COND && events[i-1].object==object && events[i-1].us==elapsed) {
            events[i-1].condition=*trace; break;
        }
}
GangstarWaitPerf gangstar_wait_stats(void) { return stats; }
GangstarWaitEvent gangstar_wait_longest(unsigned kind) {
    GangstarWaitEvent result={0};
    for (unsigned i=0;i<event_count;++i)
        if (events[i].kind==kind && events[i].us>result.us) result=events[i];
    return result;
}
void gangstar_wait_report(unsigned frame) {
    (void)frame;
    l_info("RIO_WAIT frame=%u sleep_us=%llu sleep_calls=%u mutex_us=%llu mutex_calls=%u cond_us=%llu cond_calls=%u sem_us=%llu sem_calls=%u file_us=%llu file_calls=%u net_us=%llu net_calls=%u native_us=%llu native_calls=%u",
        frame, (unsigned long long)stats.us[0], stats.calls[0],
        (unsigned long long)stats.us[1], stats.calls[1],
        (unsigned long long)stats.us[2], stats.calls[2],
        (unsigned long long)stats.us[3], stats.calls[3],
        (unsigned long long)stats.us[4], stats.calls[4],
        (unsigned long long)stats.us[5], stats.calls[5],
        (unsigned long long)stats.us[6], stats.calls[6]);
    for (unsigned i = 0; i < event_count; ++i) {
        GangstarWaitEvent *e = &events[i]; (void)e;
        l_info("RIO_WAIT_DETAIL frame=%u op=%s elapsed_us=%llu requested_us=%llu result=%d caller=0x%x object=0x%x",
            frame, e->operation, (unsigned long long)e->us, (unsigned long long)e->requested,
            e->result, (unsigned)e->caller, (unsigned)e->object);
    }
}
