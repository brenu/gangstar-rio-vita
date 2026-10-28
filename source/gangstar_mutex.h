#ifndef GANGSTAR_MUTEX_H
#define GANGSTAR_MUTEX_H
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
struct GangstarMutexWaiter;
struct GangstarMutex {
    atomic_uintptr_t acquire_site;
    atomic_int owner; unsigned depth; int kind;
    atomic_uint waiters;
    struct GangstarMutexWaiter *first,*last;
};
typedef struct {
    int (*self)(void); intptr_t (*create)(void); int (*wait)(intptr_t);
    void (*signal)(intptr_t); void (*remove)(intptr_t);
    void (*queue_lock)(void); void (*queue_unlock)(void); void (*pause)(void);
    int normal,recursive,errorcheck;
    uint64_t (*clock)(void);
} GangstarMutexOps;
int gangstar_mutex_lock(struct GangstarMutex *,const GangstarMutexOps *,uint64_t *);
int gangstar_mutex_trylock(struct GangstarMutex *,const GangstarMutexOps *);
int gangstar_mutex_unlock(struct GangstarMutex *,const GangstarMutexOps *);
#endif
