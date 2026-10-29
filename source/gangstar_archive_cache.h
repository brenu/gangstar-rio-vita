#ifndef GANGSTAR_ARCHIVE_CACHE_H
#define GANGSTAR_ARCHIVE_CACHE_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdio.h>
#ifndef RIO_CACHE_BLOCK_KIB
#define RIO_CACHE_BLOCK_KIB 128
#endif
#if RIO_CACHE_BLOCK_KIB != 32 && RIO_CACHE_BLOCK_KIB != 64 && RIO_CACHE_BLOCK_KIB != 128
#error "Supported archive cache blocks are 32, 64 or 128 KiB"
#endif
#define RIO_CACHE_BLOCK_SIZE (RIO_CACHE_BLOCK_KIB * 1024)
#ifndef RIO_CACHE_MIB
#define RIO_CACHE_MIB 16
#endif
#if RIO_CACHE_MIB != 16 && RIO_CACHE_MIB != 32 && RIO_CACHE_MIB != 64
#error "Supported archive cache sizes are 16, 32 or 64 MiB"
#endif
#define RIO_CACHE_BLOCKS (RIO_CACHE_MIB * 1024 / RIO_CACHE_BLOCK_KIB)
typedef ssize_t (*RioArchiveReadAt)(void *, void *, size_t, uint64_t);
typedef struct {
    uint64_t offset, age;
    size_t length;
    int archive, valid, loading;
} RioArchiveBlock;
typedef struct {
    uint64_t hit_bytes, miss_bytes, disk_bytes, fills;
} RioArchiveStats;
typedef struct {
    unsigned char *memory;
    unsigned block_count;
    RioArchiveBlock blocks[RIO_CACHE_BLOCKS];
    uint64_t age;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    RioArchiveStats stats;
} RioArchiveCache;
int rio_archive_cache_init(RioArchiveCache *, void *memory);
int rio_archive_cache_init_blocks(RioArchiveCache *, void *memory, unsigned blocks);
ssize_t rio_archive_cache_read(RioArchiveCache *, int archive, uint64_t file_size,
    void *destination, size_t count, uint64_t offset, RioArchiveReadAt, void *context);
RioArchiveStats rio_archive_cache_stats(RioArchiveCache *);

void gangstar_archive_init(void);
void gangstar_archive_prefetch_asset(int archive, uint64_t offset, uint64_t size);
int gangstar_archive_open_shared(const char *path); // -2: use ordinary open
void gangstar_archive_track(int fd, const char *path, int flags);
ssize_t gangstar_archive_read(int fd, void *buffer, size_t size);
ssize_t gangstar_archive_pread(int fd, void *buffer, size_t size, off_t offset);
off_t gangstar_archive_seek(int fd, off_t offset, int whence);
int gangstar_archive_close(int fd);
void gangstar_archive_forget(int fd);
int gangstar_archive_expose(int fd);
FILE *gangstar_archive_stream(FILE *original, const char *path, const char *mode);
int gangstar_archive_fileno(FILE *stream);
FILE *gangstar_archive_fdopen(int fd, const char *mode);
void gangstar_archive_report(void);
void gangstar_archive_frame(unsigned frame);
typedef struct {
    uint64_t requested, returned, elapsed_us, disk_bytes, disk_us;
    unsigned calls, disk_calls;
} GangstarArchiveFrame;
GangstarArchiveFrame gangstar_archive_frame_stats(void);
#endif
