#include "gangstar_archive_cache.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static RioArchiveCache cache;
static unsigned calls;
static int fail,interrupt,short_read;
static ssize_t read_at(void *unused,void *p,size_t n,uint64_t offset) {
 (void)unused;++calls;
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
 pthread_cond_destroy(&cache.changed);pthread_mutex_destroy(&cache.lock);free(memory);free(output);
 puts("Archive batching: one cold 256KiB read, warm reuse, byte accuracy, unaligned ranges, failure cleanup, EINTR and short reads passed");
}
