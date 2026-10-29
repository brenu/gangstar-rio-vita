/* Diagnostic forwarding wrappers. Allocation ownership stays with newlib.
 * Fixed thread slots avoid allocating TLS while tracing the allocator itself. */
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <psp2/kernel/threadmgr.h>
struct _reent;
void gangstar_host_heap_allocated(void *,size_t,uintptr_t);
void gangstar_host_heap_freed(void *);
void gangstar_host_heap_reallocated(void *,void *,size_t,uintptr_t);
typedef struct { atomic_int tid; unsigned depth; } HeapTraceThread;
static HeapTraceThread threads[128];
static HeapTraceThread *enter(void) {
    int tid=sceKernelGetThreadId();
    for (unsigned i=0;i<128;++i) {
        int owner=atomic_load_explicit(&threads[i].tid,memory_order_relaxed);
        if (!owner) {
            int empty=0;
            if (atomic_compare_exchange_strong(&threads[i].tid,&empty,tid)) owner=tid;
            else owner=empty;
        }
        if (owner==tid) { ++threads[i].depth; return &threads[i]; }
    }
    return NULL;
}
static void leave(HeapTraceThread *t) { if(t)--t->depth; }

extern void * __real_malloc(size_t size);
void * __wrap_malloc(size_t size) {
    HeapTraceThread *t=enter();
    void *result=__real_malloc(size);
    if(t && t->depth==1 && result)gangstar_host_heap_allocated(result,size,(uintptr_t)__builtin_return_address(0));
    leave(t); return result;
}

extern void * __real_calloc(size_t n,size_t size);
void * __wrap_calloc(size_t n,size_t size) {
    HeapTraceThread *t=enter();
    void *result=__real_calloc(n,size);
    if(t && t->depth==1 && result)gangstar_host_heap_allocated(result,n <= SIZE_MAX/(size?size:1) ? n*size : 0,(uintptr_t)__builtin_return_address(0));
    leave(t); return result;
}

extern void * __real_memalign(size_t align,size_t size);
void * __wrap_memalign(size_t align,size_t size) {
    HeapTraceThread *t=enter();
    void *result=__real_memalign(align,size);
    if(t && t->depth==1 && result)gangstar_host_heap_allocated(result,size,(uintptr_t)__builtin_return_address(0));
    leave(t); return result;
}

extern void * __real_realloc(void *old,size_t size);
void * __wrap_realloc(void *old,size_t size) {
    HeapTraceThread *t=enter();
    void *result=__real_realloc(old,size);
    if(t && t->depth==1 && (result || !size))gangstar_host_heap_reallocated(old,result,size,(uintptr_t)__builtin_return_address(0));
    leave(t); return result;
}

extern void __real_free(void *ptr);
void __wrap_free(void *ptr) {
    HeapTraceThread *t=enter();
    if(t && t->depth==1)gangstar_host_heap_freed(ptr);
    __real_free(ptr);
    leave(t);
}

extern void * __real__malloc_r(struct _reent *r,size_t size);
void * __wrap__malloc_r(struct _reent *r,size_t size) {
    HeapTraceThread *t=enter();
    void *result=__real__malloc_r(r,size);
    if(t && t->depth==1 && result)gangstar_host_heap_allocated(result,size,(uintptr_t)__builtin_return_address(0));
    leave(t); return result;
}

extern void * __real__calloc_r(struct _reent *r,size_t n,size_t size);
void * __wrap__calloc_r(struct _reent *r,size_t n,size_t size) {
    HeapTraceThread *t=enter();
    void *result=__real__calloc_r(r,n,size);
    if(t && t->depth==1 && result)gangstar_host_heap_allocated(result,n <= SIZE_MAX/(size?size:1) ? n*size : 0,(uintptr_t)__builtin_return_address(0));
    leave(t); return result;
}

extern void * __real__memalign_r(struct _reent *r,size_t align,size_t size);
void * __wrap__memalign_r(struct _reent *r,size_t align,size_t size) {
    HeapTraceThread *t=enter();
    void *result=__real__memalign_r(r,align,size);
    if(t && t->depth==1 && result)gangstar_host_heap_allocated(result,size,(uintptr_t)__builtin_return_address(0));
    leave(t); return result;
}

extern void * __real__realloc_r(struct _reent *r,void *old,size_t size);
void * __wrap__realloc_r(struct _reent *r,void *old,size_t size) {
    HeapTraceThread *t=enter();
    void *result=__real__realloc_r(r,old,size);
    if(t && t->depth==1 && (result || !size))gangstar_host_heap_reallocated(old,result,size,(uintptr_t)__builtin_return_address(0));
    leave(t); return result;
}

extern void __real__free_r(struct _reent *r,void *ptr);
void __wrap__free_r(struct _reent *r,void *ptr) {
    HeapTraceThread *t=enter();
    if(t && t->depth==1)gangstar_host_heap_freed(ptr);
    __real__free_r(r,ptr);
    leave(t);
}
