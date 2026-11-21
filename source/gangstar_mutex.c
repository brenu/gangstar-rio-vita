#include "gangstar_mutex.h"
#include <errno.h>
#include <limits.h>
struct GangstarMutexWaiter { struct GangstarMutexWaiter *next; intptr_t event; int thread; };
// Caller holds queue lock. Ownership is published before waking the waiter.
static void handoff(struct GangstarMutex *m,const GangstarMutexOps *o) {
    struct GangstarMutexWaiter *w=m->first; int expected=0;
    if (w && atomic_compare_exchange_strong_explicit(&m->owner,&expected,w->thread,memory_order_seq_cst,memory_order_seq_cst)) {
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
        // seq_cst pairs with the unlock fast path: either this waiter's
        // handoff sees owner==0, or the unlocker sees waiters!=0.
        atomic_fetch_add_explicit(&m->waiters,1,memory_order_seq_cst);
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
        // Uncontended release takes no global lock. The seq_cst store/load
        // pair with the waiter's seq_cst increment and handoff CAS (Dekker):
        // a waiter that enqueued after this load claims the free mutex itself.
        atomic_store_explicit(&m->owner,0,memory_order_seq_cst);
        if(!atomic_load_explicit(&m->waiters,memory_order_seq_cst))return 0;
        o->queue_lock();handoff(m,o);o->queue_unlock();
    }
    return 0;
}
