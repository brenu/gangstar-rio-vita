#include "gangstar_source_cache.h"
#include <assert.h>
#include <stdio.h>
static unsigned allocations,frees;
static volatile int fail_next;
void *__real_malloc(size_t);
void __real_free(void *);
void *__wrap_malloc(size_t n){if(fail_next){fail_next=0;return NULL;}void *p=__real_malloc(n);if(p)++allocations;return p;}
void __wrap_free(void *p){if(p)++frees;__real_free(p);}
int main(void) {
 RioSourceCache c={0};RioSourceKey a={0,1,100,32},b={0,1,200,32},d={0,1,300,32};
 const char bytes[]="test";size_t size=0;
 assert(rio_source_store(&c,a,bytes,4,8));
 void *owned=rio_source_clone(&c,a,&size);assert(owned && size==4 && !memcmp(owned,bytes,4));
 assert(rio_source_store(&c,b,"BBBB",4,8));assert(rio_source_find(&c,a));
 assert(rio_source_store(&c,d,"CCCC",4,8));
 assert(!rio_source_find(&c,b) && rio_source_find(&c,a) && c.bytes==8 && c.count==2);
 RioSourceKey mismatch=a;mismatch.original=0;assert(!rio_source_find(&c,mismatch));
 mismatch=a;mismatch.archive=1;assert(!rio_source_find(&c,mismatch));
 mismatch=a;++mismatch.source_size;assert(!rio_source_find(&c,mismatch));
 fail_next=1;assert(!rio_source_clone(&c,a,&size));assert(rio_source_find(&c,a));
 rio_source_trim(&c,0);assert(!c.bytes && !c.count && !memcmp(owned,bytes,4));free(owned);
 fail_next=1;assert(!rio_source_store(&c,a,bytes,4,8) && !c.bytes);
 assert(!rio_source_store(&c,a,bytes,4,3));
 for(unsigned i=0;i<2000;++i) {
  RioSourceKey key={0,0,i,8};assert(rio_source_store(&c,key,&key.offset,8,1024));
  assert(c.bytes<=1024 && c.count<=RIO_SOURCE_CACHE_SLOTS);
 }
 rio_source_trim(&c,0);
 for(unsigned i=0;i<500;++i) {
  RioSourceKey key={0,0,i,8};assert(rio_source_store(&c,key,&key.offset,8,8192));
  assert(c.count<=RIO_SOURCE_CACHE_SLOTS && c.bytes<=8192);
 }
 assert(c.count==RIO_SOURCE_CACHE_SLOTS && c.evictions>0);
 rio_source_trim(&c,0);assert(allocations==frees);
 const size_t mib=1024u*1024u;
 assert(rio_source_allowance(16*mib,0,100*mib,192*mib,3*mib)==16*mib);
 assert(rio_source_allowance(16*mib,16*mib,190*mib,192*mib,3*mib)==15*mib);
 assert(rio_source_allowance(16*mib,16*mib,250*mib,192*mib,3*mib)==0);
 puts("Source cache: independent owned readers, LRU eviction, exact keys, allocation failures, bounded churn and heap-pressure reclaim passed");
}
