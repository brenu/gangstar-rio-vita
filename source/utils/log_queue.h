#ifndef RIO_LOG_QUEUE_H
#define RIO_LOG_QUEUE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
enum { RIO_LOG_CAPACITY = 256 * 1024, RIO_LOG_CHUNK = 16 * 1024 };
typedef struct {
    char bytes[RIO_LOG_CAPACITY]; size_t head, used, peak;
    uint64_t dropped, enqueued;
} RioLogQueue;
// Caller serializes metadata. Whole records enter or are dropped; consumers
// copy bounded chunks then release the lock BEFORE console/storage I/O.
static inline int rio_log_push(RioLogQueue *q,const char *bytes,size_t length) {
    if (length>RIO_LOG_CAPACITY-q->used) { ++q->dropped; return 0; }
    size_t tail=(q->head+q->used)%RIO_LOG_CAPACITY;
    size_t first=RIO_LOG_CAPACITY-tail; if (first>length) first=length;
    memcpy(q->bytes+tail,bytes,first); memcpy(q->bytes,bytes+first,length-first);
    q->used+=length; q->enqueued+=length; if (q->used>q->peak) q->peak=q->used;
    return 1;
}
static inline size_t rio_log_pop(RioLogQueue *q,char *out,size_t capacity) {
    size_t n=q->used<capacity?q->used:capacity;
    size_t first=RIO_LOG_CAPACITY-q->head; if (first>n) first=n;
    memcpy(out,q->bytes+q->head,first); memcpy(out+first,q->bytes,n-first);
    q->head=(q->head+n)%RIO_LOG_CAPACITY; q->used-=n; return n;
}
#endif
