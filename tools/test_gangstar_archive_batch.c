#include "gangstar_archive_cache.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static RioArchiveCache cache;
static unsigned calls;
static size_t largest;
static int fail,interrupt,short_read;
static ssize_t read_at(void *unused,void *p,size_t n,uint64_t offset) {
 (void)unused;++calls;if(n>largest)largest=n;
 if(interrupt){interrupt=0;errno=EINTR;return -1;}
 if(fail){errno=EIO;return -1;}
 if(short_read && n>4096)n=4096;
 for(size_t i=0;i<n;++i)((unsigned char *)p)[i]=(unsigned char)((offset+i)*131u);
 return (ssize_t)n;
}
int main(void) {
 const size_t n=256u*1024u;
 void *memory=malloc(RIO_CACHE_BLOCK_SIZE*16u),*output=malloc(n);
 assert(memory && output && !rio_archive_cache_init_blocks(&cache,memory,16));
 assert(rio_archive_cache_read(&cache,0,n*8u,output,n,0,read_at,NULL)==(ssize_t)n);
 assert(calls==1); // eight 32KiB fills must become one 256KiB request
 for(size_t i=0;i<n;++i)assert(((unsigned char *)output)[i]==(unsigned char)(i*131u));
 assert(rio_archive_cache_read(&cache,0,n*8u,output,n,0,read_at,NULL)==(ssize_t)n && calls==1);
 fail=1;assert(rio_archive_cache_read(&cache,0,n*8u,output,n,n,read_at,NULL)==-1);
 for(unsigned i=0;i<cache.block_count;++i)assert(!cache.blocks[i].loading);
 fail=0;interrupt=1;short_read=1;
 assert(rio_archive_cache_read(&cache,0,n*8u,output,n,n,read_at,NULL)==(ssize_t)n);
 for(size_t i=0;i<n;++i)assert(((unsigned char *)output)[i]==(unsigned char)((n+i)*131u));
 short_read=0;
 assert(rio_archive_cache_read(&cache,0,n*8u,output,n-27,n*3+13,read_at,NULL)==(ssize_t)(n-27));
 for(size_t i=0;i<n-27;++i)assert(((unsigned char *)output)[i]==(unsigned char)((n*3+13+i)*131u));
 // Large requests bypass the cache: exact bytes, bounded physical reads,
 // no cache pollution, and an unaligned head/tail still served correctly.
 {
  const size_t big=2u*1024u*1024u+777u;const uint64_t at=5u*RIO_CACHE_BLOCK_SIZE+123u;
  unsigned char *large=malloc(big);assert(large);
  for(unsigned i=0;i<cache.block_count;++i)cache.blocks[i]=(RioArchiveBlock){0};
  calls=0;largest=0;
  assert(rio_archive_cache_read(&cache,0,n*64u,large,big,at,read_at,NULL)==(ssize_t)big);
  for(size_t i=0;i<big;++i)assert(large[i]==(unsigned char)((at+i)*131u));
  assert(largest<=RIO_CACHE_DIRECT_CHUNK);
  unsigned valid=0;for(unsigned i=0;i<cache.block_count;++i)valid+=cache.blocks[i].valid!=0;
  assert(valid<=2); // only the unaligned head and tail blocks
  RioArchiveStats st=rio_archive_cache_stats(&cache);
  assert(st.direct_bytes>=big-2u*RIO_CACHE_BLOCK_SIZE && st.direct_bytes<=big);
  // EINTR and short reads in the direct path, then a hard failure mid-way.
  interrupt=1;short_read=1;
  assert(rio_archive_cache_read(&cache,0,n*64u,large,big,0,read_at,NULL)==(ssize_t)big);
  for(size_t i=0;i<big;++i)assert(large[i]==(unsigned char)(i*131u));
  short_read=0;fail=1;
  assert(rio_archive_cache_read(&cache,0,n*64u,large,big,0,read_at,NULL)==-1);
  for(unsigned i=0;i<cache.block_count;++i)assert(!cache.blocks[i].loading);
  fail=0;free(large);
 }
 pthread_cond_destroy(&cache.changed);pthread_mutex_destroy(&cache.lock);free(memory);free(output);
 puts("Archive batching: one cold 256KiB read, warm reuse, byte accuracy, unaligned ranges, failure cleanup, EINTR and short reads, large direct reads without cache pollution passed");
}
