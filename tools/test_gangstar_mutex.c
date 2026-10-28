#define _POSIX_C_SOURCE 200809L
#include "gangstar_mutex.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <time.h>
#include <stdio.h>
static pthread_mutex_t queues=PTHREAD_MUTEX_INITIALIZER;
static atomic_int next_id=1,live,created,fail_create,fail_wait,pauses,wait_gate,entered;
static _Thread_local int identity;
static int self(void){if(!identity)identity=atomic_fetch_add(&next_id,1);return identity;}
static intptr_t create(void){
    if(atomic_exchange(&fail_create,0))return 0;
    sem_t *s=malloc(sizeof(*s));assert(s && !sem_init(s,0,0));
    atomic_fetch_add(&live,1);atomic_fetch_add(&created,1);return (intptr_t)s;
}
static int wait_event(intptr_t id){
    atomic_store(&entered,1);while(atomic_load(&wait_gate))sched_yield();
    if(atomic_exchange(&fail_wait,0))return EINTR;
    while(sem_wait((sem_t *)id))if(errno!=EINTR)return errno;
    return 0;
}
static void signal_event(intptr_t id){assert(!sem_post((sem_t *)id));}
static void remove_event(intptr_t id){assert(!sem_destroy((sem_t *)id));free((void *)id);atomic_fetch_sub(&live,1);}
static void qlock(void){pthread_mutex_lock(&queues);}static void qunlock(void){pthread_mutex_unlock(&queues);}
static void pause_thread(void){atomic_fetch_add(&pauses,1);sched_yield();}
static uint64_t clock_us(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
static const GangstarMutexOps ops={self,create,wait_event,signal_event,remove_event,qlock,qunlock,pause_thread,0,1,2,clock_us};
static struct GangstarMutex mutex;
static unsigned counter;
static void *increment(void *unused){(void)unused;for(unsigned i=0;i<5000;++i){assert(!gangstar_mutex_lock(&mutex,&ops,NULL));++counter;assert(!gangstar_mutex_unlock(&mutex,&ops));}return NULL;}
static void *single(void *unused){(void)unused;uint64_t began=0;int result=gangstar_mutex_lock(&mutex,&ops,&began);if(result)return (void *)(intptr_t)result;assert(began);++counter;assert(!gangstar_mutex_unlock(&mutex,&ops));return NULL;}
static void *wrong_owner(void *unused){(void)unused;assert(gangstar_mutex_unlock(&mutex,&ops)==EPERM);assert(gangstar_mutex_trylock(&mutex,&ops)==EBUSY);return NULL;}
int main(void){
    (void)self();uint64_t began=9;
    assert(!gangstar_mutex_lock(&mutex,&ops,&began) && !began);pthread_t t;
    assert(!pthread_create(&t,NULL,wrong_owner,NULL));pthread_join(t,NULL);
    atomic_store(&wait_gate,1);atomic_store(&fail_create,1);
    assert(!pthread_create(&t,NULL,single,NULL));while(!atomic_load(&entered))sched_yield();
    // Unlock happens before the waiter enters sem_wait: no wake may be lost.
    assert(!gangstar_mutex_unlock(&mutex,&ops));atomic_store(&wait_gate,0);pthread_join(t,NULL);
    assert(counter==1 && atomic_load(&pauses)>0 && !atomic_load(&live));
    assert(!gangstar_mutex_lock(&mutex,&ops,NULL));atomic_store(&fail_wait,1);
    assert(!pthread_create(&t,NULL,single,NULL));void *result;pthread_join(t,&result);
    assert((intptr_t)result==EINTR && !atomic_load(&mutex.waiters) && !mutex.first && !mutex.last);
    assert(!gangstar_mutex_unlock(&mutex,&ops));
    mutex.kind=1;assert(!gangstar_mutex_lock(&mutex,&ops,NULL));assert(!gangstar_mutex_trylock(&mutex,&ops));
    assert(mutex.depth==2);assert(!gangstar_mutex_unlock(&mutex,&ops));assert(mutex.depth==1);assert(!gangstar_mutex_unlock(&mutex,&ops));
    mutex.kind=2;assert(!gangstar_mutex_lock(&mutex,&ops,NULL));assert(gangstar_mutex_lock(&mutex,&ops,NULL)==EDEADLK);assert(!gangstar_mutex_unlock(&mutex,&ops));
    mutex.kind=0;counter=0;pthread_t threads[4];
    for(unsigned i=0;i<4;++i)assert(!pthread_create(&threads[i],NULL,increment,NULL));
    for(unsigned i=0;i<4;++i)pthread_join(threads[i],NULL);
    assert(counter==20000 && !atomic_load(&mutex.owner) && !atomic_load(&mutex.waiters) && !atomic_load(&live));
    assert(atomic_load(&created)>0);
    puts("Queued mutex: concurrency, ownership, recursion/errorcheck, pre-wait signal, event failure cleanup and resource exhaustion fallback passed");
}
