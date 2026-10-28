/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022      GrapheneCt
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/pthr.h"
#include "gangstar_wait_perf.h"
#include "gangstar_mutex.h"
#include "gangstar_audio_probe.h"

#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <stdatomic.h>
#include <psp2/kernel/error.h>
#include <sys/time.h>
#include <stdint.h>

#include "utils/utils.h"
#include "utils/logger.h"

#include "reimpl/bits/object_set.h"
#include <malloc.h>

#define BIONIC_PTHREAD_COND_INITIALIZER              0
#define BIONIC_PTHREAD_MUTEX_INITIALIZER             0
#define BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER   0x4000
#define BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER  0x8000

enum {
    BIONIC_PTHREAD_MUTEX_NORMAL = 0,
    BIONIC_PTHREAD_MUTEX_RECURSIVE = 1,
    BIONIC_PTHREAD_MUTEX_ERRORCHECK = 2,

    BIONIC_PTHREAD_MUTEX_ERRORCHECK_NP = BIONIC_PTHREAD_MUTEX_ERRORCHECK,
    BIONIC_PTHREAD_MUTEX_RECURSIVE_NP  = BIONIC_PTHREAD_MUTEX_RECURSIVE,

    BIONIC_PTHREAD_MUTEX_DEFAULT = BIONIC_PTHREAD_MUTEX_NORMAL
};

#define PTHR_INLINE static inline __attribute__((always_inline))

// Thousands of engine mutexes must not consume thousands of Vita UIDs.
// Ownership lives in userspace; only a thread actively waiting on a condition
// owns a temporary kernel semaphore. The registry lock protects wait queues.
struct GangstarWaiter {
    struct GangstarWaiter *next;
    SceUID semaphore;
    int signaled, tracing, signal_thread;
    uint64_t signal_time;
};
struct GangstarCondition {
    struct GangstarWaiter *first, *last;
    unsigned waiters;
};
static int sync_result(int result) {
    if (result >= 0) return 0;
    if ((unsigned)result == SCE_KERNEL_ERROR_WAIT_TIMEOUT) return ETIMEDOUT;
    l_error("Gangstar synchronization error: 0x%08x", result);
    return EINVAL;
}

static GangstarObjectSet initialized_objects;
static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;
#ifdef GANGSTAR_QUEUED_MUTEX
// Separate from the condition registry: condition_wait releases its predicate
// mutex while holding registry_lock, so reusing that lock would deadlock.
static pthread_mutex_t mutex_wait_queue_lock=PTHREAD_MUTEX_INITIALIZER;
#endif

static void report_sync_error(const char *kind, const void *object, int result) {
    struct mallinfo memory = mallinfo();
    l_error("Gangstar %s init failed: object=%p error=%d tracked=%u heap_used=%u heap_free=%u",
        kind, object, result, (unsigned)initialized_objects.count,
        (unsigned)memory.uordblks, (unsigned)memory.fordblks);
}

// null check for `attr` must be performed before this
PTHR_INLINE int _attr_t_static_init(pthread_attr_t_bionic * attr) {
    if (attr->magic != 0x42424242) {
        pthread_attr_t *real = malloc(sizeof(pthread_attr_t));
        if (!real) return ENOMEM;
        int result = pthread_attr_init(real);
        if (result != 0) { free(real); return result; }
        // pthread-embedded defaults to 4 KiB for an initialized attr object.
        // Android code expects substantially more, especially libcurl's
        // formatting frames. Hardware core 1792908326 overflowed that stack.
        result = pthread_attr_setstacksize(real, 512 * 1024);
        if (result != 0) { pthread_attr_destroy(real); free(real); return result; }
        attr->real_ptr = real;
        attr->magic = 0x42424242;
    }
    return 0;
}

// null check for `mutex` param must be performed before this, `attr` is fine as null
PTHR_INLINE int _mutex_t_static_init(pthread_mutex_t_bionic *mutex, const pthread_mutexattr_t *attr) {
    pthread_mutex_lock(&registry_lock);
    if (gangstar_object_contains(&initialized_objects, mutex)) {
        pthread_mutex_unlock(&registry_lock); return 0;
    }
    int kind = PTHREAD_MUTEX_NORMAL, result = 0;
    if (attr) result = pthread_mutexattr_gettype((pthread_mutexattr_t *)attr, &kind);
    else if (*(int *)mutex == BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_RECURSIVE;
    else if (*(int *)mutex == BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_ERRORCHECK;
    struct GangstarMutex *real = result ? NULL : calloc(1, sizeof(*real));
    if (!real && !result) result = ENOMEM;
    if (!result) {
        atomic_init(&real->owner, 0);
        atomic_init(&real->acquire_site,0);
        atomic_init(&real->waiters, 0);
        real->kind = kind;
        if (!gangstar_object_add(&initialized_objects, mutex)) result = EAGAIN;
    }
    if (result) { free(real); report_sync_error("mutex", mutex, result); }
    else mutex->real_ptr = real;
    pthread_mutex_unlock(&registry_lock);
    return result;
}

PTHR_INLINE int _cond_t_static_init(pthread_cond_t_bionic *cond, const pthread_condattr_t *attr) {
    pthread_mutex_lock(&registry_lock);
    if (gangstar_object_contains(&initialized_objects, cond)) {
        pthread_mutex_unlock(&registry_lock); return 0;
    }
    (void)attr;
    struct GangstarCondition *real = calloc(1, sizeof(*real));
    int result = real ? 0 : ENOMEM;
    if (!result && !gangstar_object_add(&initialized_objects, cond)) {
        result = EAGAIN;
    }
    if (result) { free(real); report_sync_error("condition", cond, result); }
    else cond->real_ptr = real;
    pthread_mutex_unlock(&registry_lock);
    return result;
}

int pthread_create_soloader(pthread_t *thread, const pthread_attr_t_bionic *attr, void *(*start)(void *), void *param) {
    int ret;

    if (!attr) {
        pthread_attr_t a;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, 512 * 1024);
        ret = pthread_create(thread, &a, start, param);
        pthread_attr_destroy(&a);
    } else{
        ret = _attr_t_static_init((pthread_attr_t_bionic *) attr);
        if (ret != 0) return ret;
        ret = pthread_create(thread, attr->real_ptr, start, param);
    }

    return ret;
}

int pthread_mutexattr_init_soloader(pthread_mutexattr_t *attr)
{
    return pthread_mutexattr_init(attr);
}

int pthread_mutexattr_settype_soloader(pthread_mutexattr_t *attr, int type)
{
    return pthread_mutexattr_settype(attr, type);
}

int pthread_mutexattr_destroy_soloader(pthread_mutexattr_t *attr)
{
    return pthread_mutexattr_destroy(attr);
}

int pthread_kill_soloader(pthread_t thread, int sig)
{
    return pthread_kill(thread, sig);
}

int pthread_mutex_init_soloader(pthread_mutex_t_bionic *uid, const pthread_mutexattr_t *attr)
{
    if (!uid) return EINVAL;
    return _mutex_t_static_init(uid, attr);
}

int pthread_mutex_destroy_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return 0;
    pthread_mutex_lock(&registry_lock);
    int ret = 0;
    if (gangstar_object_contains(&initialized_objects, mutex)) {
#ifdef GANGSTAR_QUEUED_MUTEX
        pthread_mutex_lock(&mutex_wait_queue_lock);
#endif
        ret = (atomic_load_explicit(&mutex->real_ptr->owner, memory_order_acquire) || atomic_load(&mutex->real_ptr->waiters)) ? EBUSY : 0;
        if (!ret) {
            gangstar_object_remove(&initialized_objects, mutex);
            free(mutex->real_ptr); mutex->real_ptr = NULL;
        }
#ifdef GANGSTAR_QUEUED_MUTEX
        pthread_mutex_unlock(&mutex_wait_queue_lock);
#endif
    }
    pthread_mutex_unlock(&registry_lock);
    return ret;
}

#ifdef GANGSTAR_QUEUED_MUTEX
static intptr_t mutex_event_create(void) {
    SceUID id=sceKernelCreateSema("Rio mutex waiter",0,0,1,NULL);
    return id<0?0:(intptr_t)id;
}
static int mutex_event_wait(intptr_t id) { return sync_result(sceKernelWaitSema((SceUID)id,1,NULL)); }
static void mutex_event_signal(intptr_t id) { sceKernelSignalSema((SceUID)id,1); }
static void mutex_event_delete(intptr_t id) { sceKernelDeleteSema((SceUID)id); }
static void mutex_queue_lock(void) { pthread_mutex_lock(&mutex_wait_queue_lock); }
static void mutex_queue_unlock(void) { pthread_mutex_unlock(&mutex_wait_queue_lock); }
static void mutex_pause(void) { sceKernelDelayThread(100); }
static const GangstarMutexOps queued_mutex_ops={sceKernelGetThreadId,mutex_event_create,mutex_event_wait,
    mutex_event_signal,mutex_event_delete,mutex_queue_lock,mutex_queue_unlock,mutex_pause,
    PTHREAD_MUTEX_NORMAL,PTHREAD_MUTEX_RECURSIVE,PTHREAD_MUTEX_ERRORCHECK,gangstar_wait_begin};
#endif

int pthread_mutex_lock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    int result = _mutex_t_static_init(mutex, NULL);
    if (result) return result;
    struct GangstarMutex *real = mutex->real_ptr;
#ifdef GANGSTAR_QUEUED_MUTEX
    int observed_owner=atomic_load_explicit(&real->owner,memory_order_relaxed);
    uintptr_t observed_site=atomic_load_explicit(&real->acquire_site,memory_order_relaxed);
    if(observed_owner!=atomic_load_explicit(&real->owner,memory_order_relaxed))observed_site=0;
    uint64_t audio_began=rio_audio_scope_active()?sceKernelGetProcessTimeWide():0;
    uint64_t queued_began=0;
    int queued_result=gangstar_mutex_lock(real,&queued_mutex_ops,&queued_began);
    if(audio_began)rio_audio_phase(RIO_AUDIO_LOCK,sceKernelGetProcessTimeWide()-audio_began);
    if(!queued_result && real->depth==1)
        atomic_store_explicit(&real->acquire_site,(uintptr_t)__builtin_return_address(0),memory_order_relaxed);
    uint64_t waited=gangstar_wait_end_context(RIO_WAIT_MUTEX,queued_began,(uintptr_t)__builtin_return_address(0),
        (uintptr_t)mutex,0,queued_result,"mutex_queue_lock",NULL,observed_owner);
    if(waited>=1000)gangstar_wait_owner_site((uintptr_t)mutex,observed_site,waited);
    return queued_result;
#else
    int self = sceKernelGetThreadId();
    unsigned spins = 0;
    uint64_t wait_started = 0;
    for (;;) {
        int expected = 0;
        if (atomic_compare_exchange_weak_explicit(&real->owner, &expected, self, memory_order_acquire, memory_order_relaxed)) {
            real->depth = 1;
            if (wait_started) gangstar_wait_end(RIO_WAIT_MUTEX, wait_started,
                (uintptr_t)__builtin_return_address(0), (uintptr_t)mutex, 0, 0, "mutex_lock");
            return 0;
        }
        if (expected == self && real->kind != PTHREAD_MUTEX_NORMAL) {
            if (real->kind == PTHREAD_MUTEX_ERRORCHECK) return EDEADLK;
            if (real->depth == UINT32_MAX) return EAGAIN;
            real->depth++; return 0;
        }
        if (++spins >= 64) {
            if (!wait_started) wait_started = gangstar_wait_begin();
            sceKernelDelayThread(100); spins = 0;
        }
    }
#endif
}

int pthread_mutex_trylock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    int result = _mutex_t_static_init(mutex, NULL);
    if (result) return result;
    struct GangstarMutex *real = mutex->real_ptr;
#ifdef GANGSTAR_QUEUED_MUTEX
    int result_try=gangstar_mutex_trylock(real,&queued_mutex_ops);
    if(!result_try && real->depth==1)
        atomic_store_explicit(&real->acquire_site,(uintptr_t)__builtin_return_address(0),memory_order_relaxed);
    return result_try;
#else
    int self = sceKernelGetThreadId(), expected = 0;
    if (atomic_compare_exchange_strong_explicit(&real->owner, &expected, self, memory_order_acquire, memory_order_relaxed)) {
        real->depth = 1; return 0;
    }
    if (expected == self && real->kind == PTHREAD_MUTEX_RECURSIVE) {
        if (real->depth == UINT32_MAX) return EAGAIN;
        real->depth++; return 0;
    }
    return EBUSY;
#endif
}

int pthread_mutex_unlock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    if (!mutex->real_ptr) return EINVAL;
    struct GangstarMutex *real = mutex->real_ptr;
#ifdef GANGSTAR_QUEUED_MUTEX
    if(atomic_load_explicit(&real->owner,memory_order_relaxed)==sceKernelGetThreadId() && real->depth==1)
        atomic_store_explicit(&real->acquire_site,0,memory_order_relaxed);
    return gangstar_mutex_unlock(real,&queued_mutex_ops);
#else
    if (atomic_load_explicit(&real->owner, memory_order_relaxed) != sceKernelGetThreadId()) return EPERM;
    if (--real->depth == 0) atomic_store_explicit(&real->owner, 0, memory_order_release);
    return 0;
#endif
}

int pthread_join_soloader(pthread_t thread, void **value_ptr)
{
    uint64_t began = gangstar_wait_begin();
    int result = pthread_join(thread, value_ptr);
    uint64_t elapsed = gangstar_wait_end(RIO_WAIT_COND, began, (uintptr_t)__builtin_return_address(0),
        (uintptr_t)thread, 0, result, "thread_join");
    return result;
}

int pthread_condattr_init_soloader(pthread_condattr_t *attr)
{
    if (!attr) return EINVAL;
    return pthread_condattr_init(attr);
}

int pthread_condattr_destroy_soloader(pthread_condattr_t *attr)
{
    if (!attr) return EINVAL;
    return pthread_condattr_destroy(attr);
}

int pthread_cond_init_soloader(pthread_cond_t_bionic *cond,
                               const pthread_condattr_t *attr)
{
    if (!cond) return EINVAL;

    return _cond_t_static_init(cond, attr);
}

int pthread_cond_destroy_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return 0;
    pthread_mutex_lock(&registry_lock);
    int ret = 0;
    if (gangstar_object_contains(&initialized_objects, cond)) {
        ret = cond->real_ptr->waiters ? EBUSY : 0;
        if (!ret) {
            gangstar_object_remove(&initialized_objects, cond);
            free(cond->real_ptr); cond->real_ptr = NULL;
        }
    }
    pthread_mutex_unlock(&registry_lock);
    return ret;
}

static int condition_signal(pthread_cond_t_bionic *cond, int all) {
    if (!cond) return EINVAL;
    int result = _cond_t_static_init(cond, NULL);
    if (result) return result;
    pthread_mutex_lock(&registry_lock);
    struct GangstarCondition *real = cond->real_ptr;
    while (real->first) {
        struct GangstarWaiter *waiter = real->first;
        uint64_t signal_time = waiter->tracing ? sceKernelGetProcessTimeWide() : 0;
        int signal_thread = waiter->tracing ? sceKernelGetThreadId() : 0;
        result = sync_result(sceKernelSignalSema(waiter->semaphore, 1));
        if (result) break;
        waiter->signaled = 1;
        waiter->signal_time = signal_time;
        waiter->signal_thread = signal_thread;
        real->first = waiter->next;
        if (!real->first) real->last = NULL;
        if (!all) break;
    }
    pthread_mutex_unlock(&registry_lock);
    return result;
}
int pthread_cond_signal_soloader(pthread_cond_t_bionic *cond) { return condition_signal(cond, 0); }
int pthread_cond_broadcast_soloader(pthread_cond_t_bionic *cond) { return condition_signal(cond, 1); }

static int condition_wait(pthread_cond_t_bionic *cond, pthread_mutex_t_bionic *mutex, struct timespec *deadline, GangstarConditionTrace *trace) {
    if (!cond || !mutex) return EINVAL;
    if (deadline && (deadline->tv_nsec < 0 || deadline->tv_nsec >= 1000000000)) return EINVAL;
    int result = _cond_t_static_init(cond, NULL);
    if (!result) result = _mutex_t_static_init(mutex, NULL);
    if (result) return result;
    struct GangstarMutex *lock = mutex->real_ptr;
    if (atomic_load_explicit(&lock->owner, memory_order_relaxed) != sceKernelGetThreadId() || lock->depth != 1) return EINVAL;
    uint64_t setup_started = gangstar_wait_begin();
    struct GangstarWaiter waiter = {.tracing = setup_started != 0};
    waiter.semaphore = sceKernelCreateSema("Gangstar waiter", 0, 0, 1, NULL);
    if (waiter.semaphore < 0) return sync_result(waiter.semaphore);
    pthread_mutex_lock(&registry_lock);
    struct GangstarCondition *real = cond->real_ptr;
    if (real->last) real->last->next = &waiter;
    else real->first = &waiter;
    real->last = &waiter;
    real->waiters++;
    uint64_t published = waiter.tracing ? sceKernelGetProcessTimeWide() : 0;
    // Publish the waiter before releasing the predicate mutex. Signals can
    // arrive before WaitSema: the semaphore retains that wakeup.
    pthread_mutex_unlock_soloader(mutex);
    pthread_mutex_unlock(&registry_lock);
    do {
        unsigned timeout, *timeout_ptr = NULL;
        if (deadline) {
            struct timeval now;
            gettimeofday(&now, NULL);
            int64_t remaining = ((int64_t)deadline->tv_sec - now.tv_sec) * 1000000 + deadline->tv_nsec / 1000 - now.tv_usec;
            timeout = remaining <= 0 ? 1 : (remaining > UINT32_MAX ? UINT32_MAX : (unsigned)remaining);
            timeout_ptr = &timeout;
        }
        result = sync_result(sceKernelWaitSema(waiter.semaphore, 1, timeout_ptr));
        if (result != ETIMEDOUT || !deadline) break;
        struct timeval now;
        gettimeofday(&now, NULL);
        if (now.tv_sec > deadline->tv_sec || (now.tv_sec == deadline->tv_sec && now.tv_usec * 1000 >= deadline->tv_nsec)) break;
    } while (1);
    uint64_t resumed = waiter.tracing ? sceKernelGetProcessTimeWide() : 0;
    pthread_mutex_lock(&registry_lock);
    if (!waiter.signaled) {
        struct GangstarWaiter *previous = NULL, *entry = real->first;
        while (entry && entry != &waiter) { previous = entry; entry = entry->next; }
        if (entry) {
            if (previous) previous->next = waiter.next;
            else real->first = waiter.next;
            if (real->last == &waiter) real->last = previous;
        }
    } else result = 0;
    real->waiters--;
    sceKernelDeleteSema(waiter.semaphore);
    pthread_mutex_unlock(&registry_lock);
    uint64_t relock_started = waiter.tracing ? sceKernelGetProcessTimeWide() : 0;
    int relocked = pthread_mutex_lock_soloader(mutex);
    if (waiter.tracing) {
        uint64_t end = sceKernelGetProcessTimeWide();
        *trace = (GangstarConditionTrace){
            .setup_us=published-setup_started,
            .before_signal_us=waiter.signaled && waiter.signal_time>=published ? waiter.signal_time-published : 0,
            .resume_us=waiter.signaled && resumed>=waiter.signal_time ? resumed-waiter.signal_time : 0,
            .cleanup_us=relock_started-resumed, .relock_us=end-relock_started,
            .signal_thread=waiter.signal_thread, .signaled=waiter.signaled};
    }
    return relocked ? relocked : result;
}

int pthread_cond_timedwait_soloader(pthread_cond_t_bionic *cond, pthread_mutex_t_bionic *mutex, struct timespec *abstime) {
    if (!abstime) return EINVAL;
    uint64_t began = gangstar_wait_begin();
    GangstarConditionTrace trace={0};
    int result = condition_wait(cond, mutex, abstime, &trace);
    uint64_t elapsed = gangstar_wait_end(RIO_WAIT_COND, began, (uintptr_t)__builtin_return_address(0),
        (uintptr_t)cond, 0, result, "cond_timedwait");
    gangstar_wait_condition((uintptr_t)cond, elapsed, &trace);
    return result;
}
int pthread_cond_wait_soloader(pthread_cond_t_bionic *cond, pthread_mutex_t_bionic *mutex) {
    uint64_t began = gangstar_wait_begin();
    GangstarConditionTrace trace={0};
    int result = condition_wait(cond, mutex, NULL, &trace);
    uint64_t elapsed = gangstar_wait_end(RIO_WAIT_COND, began, (uintptr_t)__builtin_return_address(0),
        (uintptr_t)cond, 0, result, "cond_wait");
    gangstar_wait_condition((uintptr_t)cond, elapsed, &trace);
    return result;
}

// Run before loading the Android library: verify the resource count that
// previously exhausted semaphores, plus wakeup and timeout lock ownership.
// Only active waiters need a kernel UID; idle pairs stay in userspace.
struct SyncProbe {
    pthread_mutex_t_bionic mutex;
    pthread_cond_t_bionic cond;
    int ready;
};
static void *sync_probe_worker(void *arg) {
    struct SyncProbe *probe = arg;
    int result = pthread_mutex_lock_soloader(&probe->mutex);
    if (!result) {
        probe->ready = 1;
        result = pthread_cond_signal_soloader(&probe->cond);
        int unlocked = pthread_mutex_unlock_soloader(&probe->mutex);
        if (!result) result = unlocked;
    }
    return (void *)(uintptr_t)result;
}
int gangstar_sync_selftest(void) {
    const unsigned count = 1100;
    struct SyncProbe *probes = calloc(count, sizeof(*probes));
    if (!probes) return ENOMEM;
    int result = 0;
    for (unsigned i = 0; i < count && !result; ++i) {
        result = pthread_cond_init_soloader(&probes[i].cond, NULL);
        if (!result) {
            result = pthread_mutex_lock_soloader(&probes[i].mutex);
            if (result) break;
            struct timeval now;
            gettimeofday(&now, NULL);
            struct timespec deadline = {now.tv_sec, now.tv_usec * 1000};
            result = pthread_cond_timedwait_soloader(&probes[i].cond, &probes[i].mutex, &deadline);
            int unlocked = pthread_mutex_unlock_soloader(&probes[i].mutex);
            result = result == ETIMEDOUT ? unlocked : EINVAL;
        }
    }
    if (!result) {
        struct SyncProbe *probe = &probes[0];
        pthread_t worker;
        result = pthread_mutex_lock_soloader(&probe->mutex);
        if (!result) {
            int created = pthread_create_soloader(&worker, NULL, sync_probe_worker, probe);
            result = created;
            while (!result && !probe->ready) {
                struct timeval now;
                gettimeofday(&now, NULL);
                struct timespec deadline = {now.tv_sec + 2, now.tv_usec * 1000};
                result = pthread_cond_timedwait_soloader(&probe->cond, &probe->mutex, &deadline);
            }
            int unlocked = pthread_mutex_unlock_soloader(&probe->mutex);
            if (!result) result = unlocked;
            if (!created) {
                void *worker_result = NULL;
                int joined = pthread_join(worker, &worker_result);
                if (!result) result = joined ? joined : (int)(uintptr_t)worker_result;
            }
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        int destroyed = pthread_cond_destroy_soloader(&probes[i].cond);
        if (!result) result = destroyed;
        destroyed = pthread_mutex_destroy_soloader(&probes[i].mutex);
        if (!result) result = destroyed;
    }
    free(probes);
    l_info("Gangstar synchronization self-test: pairs=%u result=%d tracked=%u", count, result, (unsigned)initialized_objects.count);
    return result;
}

int pthread_attr_init_soloader(pthread_attr_t_bionic *attr)
{
    if (!attr) return EINVAL;

    return _attr_t_static_init(attr);
}

int pthread_attr_destroy_soloader(pthread_attr_t_bionic *attr)
{
    if (!attr) return 0;
    if (attr->magic != 0x42424242) return 0;

    int ret = pthread_attr_destroy(attr->real_ptr);
    free(attr->real_ptr);
    attr->magic = 0x0;

    return ret;
}

int pthread_attr_setdetachstate_soloader(pthread_attr_t_bionic *attr, int state)
{
    if (!attr) return -1;
    _attr_t_static_init(attr);
    state = !state; // pthread-embedded has JOINABLE/DETACHED swapped compared to BIONIC...
    return pthread_attr_setdetachstate(attr->real_ptr, state);
}

int pthread_attr_setstacksize_soloader(pthread_attr_t_bionic *attr, size_t stacksize) {
    if (!attr) return -1;
    int result = _attr_t_static_init(attr);
    if (result != 0) return result;
    if (stacksize < 512 * 1024) stacksize = 512 * 1024;
    return pthread_attr_setstacksize(attr->real_ptr, stacksize);
}

int pthread_attr_setstack_soloader(pthread_attr_t_bionic *attr, void *stackaddr, size_t stacksize) {
    if (!attr || !stackaddr || !stacksize) return EINVAL;
    int result = _attr_t_static_init(attr);
    if (result != 0) return result;
    return pthread_attr_setstack(attr->real_ptr, stackaddr, stacksize);
}

int pthread_setschedparam_soloader(pthread_t thread, int policy,
                                   const struct sched_param *param)
{
   return pthread_setschedparam(thread, policy, param);
}

int pthread_getschedparam_soloader(pthread_t thread, int *policy,
                                   struct sched_param *param)
{
    return pthread_getschedparam(thread, policy, param);
}

int pthread_detach_soloader(pthread_t thread)
{
    return pthread_detach(thread);
}

int pthread_equal_soloader(const pthread_t t1, const pthread_t t2)
{
    if (t1 == t2)
        return 1;
    if (!t1 || !t2)
        return 0;
    return pthread_equal(t1, t2);
}

pthread_t pthread_self_soloader()
{
    return pthread_self();
}

int pthread_once_soloader(volatile int *once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine)
        return -1;
    if (__sync_lock_test_and_set(once_control, 1) == 0)
        (*init_routine)();
    return 0;
}

#ifndef MAX_TASK_COMM_LEN
#define MAX_TASK_COMM_LEN 16
#endif

int pthread_setname_np_soloader(pthread_t thread, const char* thread_name) {
    if (thread == 0 || thread_name == NULL) {
        return EINVAL;
    }
    size_t thread_name_len = strlen(thread_name);
    if (thread_name_len >= MAX_TASK_COMM_LEN) {
        return ERANGE;
    }

    sceClibPrintf("PTHREAD: pthread_setname_np with name %s for thread:0x%x\n", thread_name, pthread_self());

    return 0;
}

int sem_destroy_soloader(int * uid) {
    if (sceKernelDeleteSema(*uid) < 0)
        return -1;
    return 0;
}

int sem_getvalue_soloader (int * uid, int * sval) {
    SceKernelSemaInfo info;
    info.size = sizeof(SceKernelSemaInfo);

    if (sceKernelGetSemaInfo(*uid, &info) < 0) return -1;
    if (!sval) sval = malloc(sizeof(int32_t));
    *sval = info.currentCount;
    return 0;
}

int sem_init_soloader (int * uid, int pshared, unsigned int value) {
    *uid = sceKernelCreateSema("sema", 0, (int) value, 0x7fffffff, NULL);
    if (*uid < 0)
        return -1;
    return 0;
}

int sem_post_soloader (int * uid) {
    if (sceKernelSignalSema(*uid, 1) < 0)
        return -1;
    return 0;
}

static int sem_timedwait_impl (int * uid, const struct timespec * abstime) {
    uint timeout = 1000;
    if (sceKernelWaitSema(*uid, 1, &timeout) >= 0)
        return 0;
    if (!abstime) return -1;
    long long now = (long long) current_timestamp_ms() * 1000; // us
    long long _timeout = abstime->tv_sec * 1000 * 1000 + abstime->tv_nsec / 1000; // us
    if (_timeout-now >= 0) return -1;
    uint timeout_real = _timeout - now;
    if (sceKernelWaitSema(*uid, 1, &timeout_real) < 0)
        return -1;
    return 0;
}

int sem_timedwait_soloader(int *uid, const struct timespec *abstime) {
    uint64_t began = gangstar_wait_begin();
    int result = sem_timedwait_impl(uid, abstime);
    gangstar_wait_end(RIO_WAIT_SEM, began, (uintptr_t)__builtin_return_address(0),
        (uintptr_t)uid, 0, result, "sem_timedwait");
    return result;
}

int sem_trywait_soloader (int * uid) {
    uint64_t began = gangstar_wait_begin();
    uint timeout = 1000;
    int result = sceKernelWaitSema(*uid, 1, &timeout) < 0 ? -1 : 0;
    gangstar_wait_end(RIO_WAIT_SEM, began, (uintptr_t)__builtin_return_address(0),
        (uintptr_t)uid, 1000, result, "sem_trywait");
    return result;
}

int sem_wait_soloader (int * uid) {
    uint64_t began = gangstar_wait_begin();
    int result = sceKernelWaitSema(*uid, 1, NULL) < 0 ? -1 : 0;
    gangstar_wait_end(RIO_WAIT_SEM, began, (uintptr_t)__builtin_return_address(0),
        (uintptr_t)uid, 0, result, "sem_wait");
    return result;
}
