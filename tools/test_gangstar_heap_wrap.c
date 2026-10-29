#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct _reent;
int sceKernelGetThreadId(void) { return 1; }
static unsigned allocs,frees,reallocs;
static int fail_realloc;
void gangstar_host_heap_allocated(void *p,size_t n,uintptr_t c) { assert(p && n && c); ++allocs; }
void gangstar_host_heap_freed(void *p) { if(p)++frees; }
void gangstar_host_heap_reallocated(void *old,void *p,size_t n,uintptr_t c) { assert(old && p && n && c);++reallocs; }
void * __wrap_malloc(size_t);
void * __wrap_calloc(size_t,size_t);
void * __wrap_memalign(size_t,size_t);
void * __wrap_realloc(void *,size_t);
void __wrap_free(void *);
void *__real_malloc(size_t n) { return malloc(n); }
void *__real_calloc(size_t n,size_t z) { void *p=__wrap_malloc(n*z); if(p)memset(p,0,n*z);return p; }
void *__real_memalign(size_t a,size_t n) { void *p=NULL;return posix_memalign(&p,a,n)?NULL:p; }
void *__real_realloc(void *p,size_t n) { return fail_realloc?NULL:realloc(p,n); }
void __real_free(void *p) { free(p); }
void * __real__malloc_r(struct _reent *r,size_t n) { (void)r; return __wrap_malloc(n); }
void * __real__calloc_r(struct _reent *r,size_t n,size_t z) { (void)r; return __wrap_calloc(n,z); }
void * __real__memalign_r(struct _reent *r,size_t a,size_t n) { (void)r; return __wrap_memalign(a,n); }
void * __real__realloc_r(struct _reent *r,void *p,size_t n) { (void)r; return __wrap_realloc(p,n); }
void __real__free_r(struct _reent *r,void *p) { (void)r; __wrap_free(p); }
void *__wrap__malloc_r(struct _reent *,size_t);
int main(void) {
    void *p=__wrap_malloc(64);assert(p && allocs==1);
    fail_realloc=1;assert(!__wrap_realloc(p,128) && reallocs==0);
    memset(p,1,64);fail_realloc=0;
    p=__wrap_realloc(p,128);assert(p && reallocs==1);
    __wrap_free(p);assert(frees==1);
    p=__wrap_calloc(4,16);assert(p && allocs==2);
    for(unsigned i=0;i<64;++i)assert(((unsigned char *)p)[i]==0);
    __wrap_free(p);
    p=__wrap_memalign(64,128);assert(p && !((uintptr_t)p&63) && allocs==3);__wrap_free(p);
    p=__wrap__malloc_r(NULL,64);assert(p && allocs==4);__wrap_free(p);
    assert(frees==4);
    puts("Heap wrappers: forwarding, alignment, nested calls counted once and failed realloc ownership passed");
}
