#include "gangstar_mutex.h"
#include <errno.h>
#include <limits.h>
struct GangstarMutexWaiter { struct GangstarMutexWaiter *next; intptr_t event; int thread; };
// Caller holds queue lock. Ownership is published before waking the waiter.
static void handoff(struct GangstarMutex *m,const GangstarMutexOps *o) {
    struct GangstarMutexWaiter *w=m->first; int expected=0;
    if (w && atomic_compare_exchange_strong_explicit(&m->owner,&expected,w->thread,memory_order_acquire,memory_order_relaxed)) {
        m->first=w->next;if(!m->first)m->last=NULL;
        m->depth=1;o->signal(w->event);
    }
}
int gangstar_mutex_trylock(struct GangstarMutex *m,const GangstarMutexOps *o) {
    int self=o->self(),expected=0;
    if(atomic_compare_exchange_strong_explicit(&m->owner,&expected,self,memory_order_acquire,memory_order_relaxed)) {
        m->depth=1;return 0;
    }
    if(expected==self && m->kind==o->recursive) {
        if(m->depth==UINT_MAX)return EAGAIN;
        ++m->depth;return 0;
    }
    return EBUSY;
}
int gangstar_mutex_lock(struct GangstarMutex *m,const GangstarMutexOps *o,uint64_t *began) {
    if(began)*began=0;
    int self=o->self();unsigned spins=0;
    for(;;) {
        int expected=0;
        if(atomic_compare_exchange_weak_explicit(&m->owner,&expected,self,memory_order_acquire,memory_order_relaxed)) {
            m->depth=1;return 0;
        }
        if(expected==self && m->kind!=o->normal) {
            if(m->kind==o->errorcheck)return EDEADLK;
            if(m->depth==UINT_MAX)return EAGAIN;
            ++m->depth;return 0;
        }
        if(++spins<64)continue;
        spins=0;
        if(began && !*began && o->clock)*began=o->clock();
        intptr_t event=o->create();
        if(!event) { o->pause();continue; } // Preserve progress if waiter resources are exhausted.
        struct GangstarMutexWaiter w={NULL,event,self};
        o->queue_lock();
        atomic_fetch_add_explicit(&m->waiters,1,memory_order_release);
        if(m->last)m->last->next=&w;else m->first=&w;
        m->last=&w;
        // Handles unlock-before-enqueue and unlock-before-wait; the event keeps
        // a signal even if the waiting thread has not entered the kernel yet.
        handoff(m,o);
        o->queue_unlock();
        int result=o->wait(event);
        o->queue_lock();
        if(result) {
            struct GangstarMutexWaiter *prev=NULL,*it=m->first;
            while(it && it!=&w){prev=it;it=it->next;}
            if(it) { if(prev)prev->next=w.next;else m->first=w.next;if(m->last==&w)m->last=prev; }
            if(atomic_load_explicit(&m->owner,memory_order_relaxed)==self) {
                m->depth=0;atomic_store_explicit(&m->owner,0,memory_order_release);handoff(m,o);
            }
        }
        atomic_fetch_sub_explicit(&m->waiters,1,memory_order_release);
        o->queue_unlock();o->remove(event);
        // Acquire the release sequence from the previous owner before returning.
        if(!result && atomic_load_explicit(&m->owner,memory_order_acquire)!=self)return EINVAL;
        return result;
    }
}
int gangstar_mutex_unlock(struct GangstarMutex *m,const GangstarMutexOps *o) {
    if(atomic_load_explicit(&m->owner,memory_order_relaxed)!=o->self())return EPERM;
    if(--m->depth==0) {
        // Serialize release with enqueue/recheck, preventing a lost wakeup
        // even on ARM's weak memory ordering. No I/O occurs under this lock.
        o->queue_lock();
        atomic_store_explicit(&m->owner,0,memory_order_release);
        handoff(m,o);o->queue_unlock();
    }
    return 0;
}
