#define _XOPEN_SOURCE 700
#include "gangstar_archive_cache.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include <errno.h>
static void *memory;
static atomic_uint lseek_calls, position_queries;
static atomic_int fail_set;
off_t __real_lseek(int,off_t,int);
off_t __wrap_lseek(int fd,off_t offset,int whence) { ++lseek_calls;if(whence==SEEK_SET && atomic_exchange(&fail_set,0)) { errno=EIO;return -1; }if(offset==0 && whence==SEEK_CUR)++position_queries;return __real_lseek(fd,offset,whence); }
int sceKernelAllocMemBlock(const char *name, int type, size_t size, void *opt) {
    (void)name; (void)type; (void)opt;
#ifdef RIO_TEST_FAIL_LARGE
    if(size>16*1024*1024)return -1;
#endif
    memory = calloc(1, size); return memory ? 1 : -1;
}
int sceKernelGetMemBlockBase(int id, void **out) { (void)id; *out = memory; return 0; }
int sceKernelFreeMemBlock(int id) { (void)id; free(memory); return 0; }
void _log_print(int t, const char *fmt, ...) { (void)t; (void)fmt; }
static const char *archive = "ux0:/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb";
static const off_t length = 2119380688;
static const char marker[] = "block-boundary-marker";
static void *shared_reader(void *argument) {
    int fd = *(int *)argument;
    char bytes[37];
    for (unsigned i = 0; i < 100; ++i) {
        assert(gangstar_archive_read(fd, bytes, sizeof(bytes)) == sizeof(bytes));
        for (unsigned j = 0; j < sizeof(bytes); ++j) assert(bytes[j] == 0);
    }
    return NULL;
}
int main(void) {
    char path[] = "/tmp/rio-cache-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0);
    assert(!ftruncate(fd, length));
    assert(pwrite(fd, marker, sizeof(marker), 0) == sizeof(marker));
    assert(pwrite(fd, marker, sizeof(marker), RIO_CACHE_BLOCK_SIZE - 4) == sizeof(marker));
    assert(pwrite(fd, marker, sizeof(marker), length - sizeof(marker)) == sizeof(marker));
    close(fd);
    gangstar_archive_init();
    fd = open(path, O_RDONLY); assert(fd >= 0); gangstar_archive_track(fd, archive, O_RDONLY);
    char bytes[64];
    assert(gangstar_archive_read(fd, bytes, sizeof(marker)) == sizeof(marker) && !memcmp(bytes, marker, sizeof(marker)));
    assert(gangstar_archive_pread(fd, bytes, sizeof(marker), RIO_CACHE_BLOCK_SIZE - 4) == sizeof(marker));
    assert(!memcmp(bytes, marker, sizeof(marker)));
    assert(gangstar_archive_seek(fd, 0, SEEK_CUR) == sizeof(marker));
    assert(gangstar_archive_seek(fd, -(off_t)sizeof(marker), SEEK_END) == length - sizeof(marker));
    assert(gangstar_archive_read(fd, bytes, sizeof(bytes)) == sizeof(marker));
    assert(!memcmp(bytes, marker, sizeof(marker)) && gangstar_archive_read(fd, bytes, 1) == 0);
    assert(gangstar_archive_seek(fd, 2 * RIO_CACHE_BLOCK_SIZE, SEEK_SET) == 2 * RIO_CACHE_BLOCK_SIZE);
    pthread_t threads[4];
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_create(&threads[i], NULL, shared_reader, &fd));
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_join(threads[i], NULL));
    assert(gangstar_archive_seek(fd, 0, SEEK_CUR) == 2 * RIO_CACHE_BLOCK_SIZE + 4 * 100 * 37);
    assert(!gangstar_archive_close(fd));
    fd = open(path, O_RDONLY); gangstar_archive_track(fd, archive, O_RDONLY);
    assert(gangstar_archive_read(fd, bytes, sizeof(marker)) == sizeof(marker));
    assert(!memcmp(bytes, marker, sizeof(marker)));
    // fdopen/fclose ownership transfer removes the cache descriptor mapping.
    FILE *stream = fdopen(fd, "rb"); assert(stream);
    gangstar_archive_forget(fd); assert(!fclose(stream));
    fd = open(path, O_RDONLY);
    assert(gangstar_archive_read(fd, bytes, sizeof(marker)) == sizeof(marker));
    assert(!memcmp(bytes, marker, sizeof(marker))); assert(!gangstar_archive_close(fd));
    stream = gangstar_archive_stream(fopen(path, "rb"), archive, "rb"); assert(stream);
    assert(fread(bytes, 1, sizeof(marker), stream) == sizeof(marker));
    assert(!memcmp(bytes, marker, sizeof(marker)) && ftell(stream) == sizeof(marker));
    unsigned before_queries=lseek_calls;
    for(unsigned i=0;i<32;++i)assert(ftell(stream)==sizeof(marker));
#if defined(GANGSTAR_ARCHIVE_POSITION_CACHE) || defined(GANGSTAR_ARCHIVE_VIRTUAL_CURSOR)
    assert(lseek_calls==before_queries); /* No backend query even with stdio read-ahead. */
#else
    assert(lseek_calls>before_queries);
#endif
    int next=fgetc(stream);assert(next!=EOF);
    assert(ftell(stream)==sizeof(marker)+1);
    assert(ungetc(next,stream)==next && ftell(stream)==sizeof(marker));
    assert(fgetc(stream)==next && ftell(stream)==sizeof(marker)+1);
    assert(!fseek(stream,sizeof(marker),SEEK_SET));
    assert(ftell(stream)==sizeof(marker));
    int exposed=gangstar_archive_fileno(stream);assert(exposed>=0);
    before_queries=lseek_calls;assert(ftell(stream)==sizeof(marker));
    assert(lseek_calls>before_queries); /* Exposed descriptors always query real position. */
    int alias=dup(exposed);assert(alias>=0);
    assert(lseek(alias,12345,SEEK_SET)==12345);
    assert(gangstar_archive_seek(exposed,0,SEEK_CUR)==12345);
    close(alias);
    assert(!fseek(stream, RIO_CACHE_BLOCK_SIZE - 4, SEEK_SET));
    assert(fread(bytes, 1, sizeof(marker), stream) == sizeof(marker));
    assert(!memcmp(bytes, marker, sizeof(marker)));
    assert(!fseek(stream, -5, SEEK_END));
    assert(fread(bytes, 4, 2, stream) == 1 && feof(stream) && !ferror(stream));
    assert(ftell(stream) == length);
    rewind(stream); assert(!feof(stream));
    assert(fread(bytes, 1, sizeof(marker), stream) == sizeof(marker));
    assert(!memcmp(bytes, marker, sizeof(marker))); assert(!fclose(stream));
    fd = open(path, O_RDONLY); gangstar_archive_track(fd, archive, O_RDONLY);
    stream = gangstar_archive_fdopen(fd, "rb"); assert(stream);
    assert(gangstar_archive_fileno(stream) == fd);
    assert(fread(bytes, 1, sizeof(marker), stream) == sizeof(marker));
    assert(!memcmp(bytes, marker, sizeof(marker))); assert(!fclose(stream));
    /* Force backend reads rather than stdio read-ahead. Only private cookies
       can reuse position, and real descriptor position still advances. */
    stream=gangstar_archive_stream(fopen(path,"rb"),archive,"rb");assert(stream);
    assert(!setvbuf(stream,NULL,_IONBF,0));
    assert(fgetc(stream)==marker[0]);
    unsigned before_reads=position_queries;
    unsigned before_advances=lseek_calls;(void)before_advances;
    for(unsigned i=1;i<sizeof(marker);++i)assert(fgetc(stream)==marker[i]);
#if defined(GANGSTAR_ARCHIVE_READ_POSITION_CACHE) || defined(GANGSTAR_ARCHIVE_VIRTUAL_CURSOR)
    assert(position_queries==before_reads);
#else
    assert(position_queries>before_reads);
#endif
#ifdef GANGSTAR_ARCHIVE_VIRTUAL_CURSOR
    assert(lseek_calls==before_advances); /* Neither query nor advance hits kernel. */
    fail_set=1;errno=0;
    assert(gangstar_archive_fileno(stream)==-1 && errno==EIO && !fail_set);
    assert(ftell(stream)==sizeof(marker)); /* Failed exposure retained logical offset. */
    assert(fgetc(stream)==0);
    fail_set=1;errno=0;
    assert(fseek(stream,0,SEEK_SET)==-1 && errno==EIO && !fail_set);
    assert(ftell(stream)==sizeof(marker)+1);
    assert(fgetc(stream)==0);
    assert(!fseek(stream,-2,SEEK_CUR));
    assert(ftell(stream)==sizeof(marker));
    assert(fgetc(stream)==0);assert(!fseek(stream,-1,SEEK_CUR));
#endif
    exposed=gangstar_archive_fileno(stream);assert(exposed>=0);
    assert(__real_lseek(exposed,0,SEEK_CUR)==sizeof(marker));
    alias=dup(exposed);assert(alias>=0);
    assert(lseek(alias,12345,SEEK_SET)==12345);
    before_reads=position_queries;
    assert(fgetc(stream)==0 && position_queries>before_reads);
    assert(__real_lseek(alias,0,SEEK_CUR)==12346);
    close(alias);assert(!fclose(stream));
    unlink(path);
    char other[] = "/tmp/rio-plain-XXXXXX";
    fd = mkstemp(other); assert(fd >= 0); assert(write(fd, "plain", 5) == 5);
    assert(lseek(fd, 0, SEEK_SET) == 0);
    assert(gangstar_archive_read(fd, bytes, 5) == 5 && !memcmp(bytes, "plain", 5));
    assert(!gangstar_archive_close(fd)); unlink(other);
    assert(gangstar_archive_read(-1, bytes, 1) == -1);
    assert(gangstar_archive_close(-1) == -1);
    free(memory);
    puts("Archive I/O: seek/pread positions, large sparse archive EOF, reopen, fdopen ownership and untracked descriptor fallback passed");
}
