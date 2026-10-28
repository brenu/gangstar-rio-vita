/* Immutable archive blocks shared across independent descriptor lifetimes. */
#include "gangstar_archive_cache.h"
#include <string.h>
#include <errno.h>
#include <limits.h>

int rio_archive_cache_init(RioArchiveCache *cache, void *memory) {
    return rio_archive_cache_init_blocks(cache,memory,RIO_CACHE_BLOCKS);
}
int rio_archive_cache_init_blocks(RioArchiveCache *cache, void *memory, unsigned blocks) {
    if(!memory || !blocks || blocks>RIO_CACHE_BLOCKS)return EINVAL;
    memset(cache, 0, sizeof(*cache));
    int result = pthread_mutex_init(&cache->lock, NULL);
    if (result) return result;
    result = pthread_cond_init(&cache->changed, NULL);
    if (result) pthread_mutex_destroy(&cache->lock);
    else { cache->memory = memory;cache->block_count=blocks; }
    return result;
}

ssize_t rio_archive_cache_read(RioArchiveCache *cache, int archive, uint64_t file_size,
    void *destination, size_t count, uint64_t offset, RioArchiveReadAt read_at, void *context) {
    if (count > SIZE_MAX / 2) { errno = EINVAL; return -1; }
    if (count && !destination) { errno = EFAULT; return -1; }
    if (!count || offset >= file_size) return 0;
    if ((uint64_t)count > file_size - offset) count = (size_t)(file_size - offset);
    size_t done = 0;
    while (done < count) {
        uint64_t block_offset = offset / RIO_CACHE_BLOCK_SIZE * RIO_CACHE_BLOCK_SIZE;
        size_t within = (size_t)(offset - block_offset);
        size_t take = RIO_CACHE_BLOCK_SIZE - within;
        if (take > count - done) take = count - done;
        // Metadata/copy lock never covers disk I/O. Loading blocks are pinned;
        // matching requests wait for that fill, independent requests proceed.
        pthread_mutex_lock(&cache->lock);
retry: ;
        unsigned victim = cache->block_count;
        int found = 0;
        for (unsigned i = 0; i < cache->block_count; ++i) {
            RioArchiveBlock *b = &cache->blocks[i];
            if (b->loading) {
                if (b->archive == archive && b->offset == block_offset) {
                    pthread_cond_wait(&cache->changed, &cache->lock);
                    goto retry;
                }
                continue;
            }
            if (b->valid && b->archive == archive && b->offset == block_offset) {
                victim = i; found = 1; break;
            }
            if (victim == cache->block_count || !b->valid ||
                (cache->blocks[victim].valid && b->age < cache->blocks[victim].age))
                victim = i;
        }
        if (victim == cache->block_count) {
            pthread_cond_wait(&cache->changed, &cache->lock);
            goto retry;
        }
        RioArchiveBlock *block = &cache->blocks[victim];
        if (!found) {
            *block = (RioArchiveBlock){.archive = archive, .offset = block_offset, .loading = 1};
            pthread_mutex_unlock(&cache->lock);
            size_t needed = RIO_CACHE_BLOCK_SIZE;
            if (file_size - block_offset < needed) needed = (size_t)(file_size - block_offset);
            size_t loaded = 0;
            int error = 0;
            while (loaded < needed) {
                ssize_t n = read_at(context, cache->memory + victim * RIO_CACHE_BLOCK_SIZE + loaded,
                    needed - loaded, block_offset + loaded);
                if (n < 0) { if (errno == EINTR) continue; error = 1; break; }
                if (!n) break;
                if ((size_t)n > needed - loaded) { errno = EIO; error = 1; break; }
                loaded += (size_t)n;
            }
            pthread_mutex_lock(&cache->lock);
            cache->stats.disk_bytes += loaded;
            if (error || loaded != needed) {
                if (!error) errno = EIO; // archive changed/truncated; do not cache it
                block->loading = 0;
                pthread_cond_broadcast(&cache->changed);
                pthread_mutex_unlock(&cache->lock);
                return done ? (ssize_t)done : -1;
            }
            *block = (RioArchiveBlock){.archive = archive, .offset = block_offset,
                .length = loaded, .valid = 1};
            ++cache->stats.fills;
            cache->stats.miss_bytes += take;
            pthread_cond_broadcast(&cache->changed);
        } else cache->stats.hit_bytes += take;
        memcpy((unsigned char *)destination + done,
            cache->memory + victim * RIO_CACHE_BLOCK_SIZE + within, take);
        block->age = ++cache->age;
        pthread_mutex_unlock(&cache->lock);
        done += take; offset += take;
    }
    return (ssize_t)done;
}

RioArchiveStats rio_archive_cache_stats(RioArchiveCache *cache) {
    pthread_mutex_lock(&cache->lock);
    RioArchiveStats stats = cache->stats;
    cache->stats = (RioArchiveStats){0};
    pthread_mutex_unlock(&cache->lock);
    return stats;
}
