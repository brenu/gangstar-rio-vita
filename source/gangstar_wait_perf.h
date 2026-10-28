#ifndef GANGSTAR_WAIT_PERF_H
#define GANGSTAR_WAIT_PERF_H
#include <stdint.h>
enum { RIO_WAIT_SLEEP, RIO_WAIT_MUTEX, RIO_WAIT_COND, RIO_WAIT_SEM, RIO_WAIT_FILE, RIO_WAIT_NET, RIO_WAIT_NATIVE, RIO_WAIT_KINDS };
typedef struct { uint64_t us[RIO_WAIT_KINDS]; unsigned calls[RIO_WAIT_KINDS]; } GangstarWaitPerf;
typedef struct {
    uint64_t setup_us, before_signal_us, resume_us, cleanup_us, relock_us;
    int signal_thread, signaled;
} GangstarConditionTrace;
typedef struct {
    GangstarConditionTrace condition;
    uint64_t us, requested; uintptr_t caller, object;
    int result; const char *operation; unsigned kind;
    int saved_errno, observed_owner;
    uintptr_t owner_acquire_site;
    char path[192];
} GangstarWaitEvent;
GangstarWaitEvent gangstar_wait_longest(unsigned kind);
void gangstar_wait_frame(void);
uint64_t gangstar_wait_begin(void);
int gangstar_wait_is_main(void);
uint64_t gangstar_wait_end(unsigned kind, uint64_t began, uintptr_t caller, uintptr_t object,
    uint64_t requested, int result, const char *operation);
uint64_t gangstar_wait_end_context(unsigned kind, uint64_t began, uintptr_t caller, uintptr_t object,
    uint64_t requested, int result, const char *operation, const char *path, int observed_owner);
void gangstar_wait_owner_site(uintptr_t object,uintptr_t site,uint64_t elapsed);
void gangstar_wait_condition(uintptr_t object, uint64_t elapsed, const GangstarConditionTrace *trace);
GangstarWaitPerf gangstar_wait_stats(void);
void gangstar_wait_report(unsigned frame);
#endif
