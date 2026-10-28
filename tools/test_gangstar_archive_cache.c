#include "gangstar_archive_cache.h"
#include "gangstar_prefetch_policy.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

static pthread_mutex_t gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_changed = PTHREAD_COND_INITIALIZER;
static int entered, unblock, probe_done;

static RioArchiveCache cache;
static const uint64_t length = UINT64_C(2119380688);
struct Source { unsigned archive, calls; int fail, interrupt; };
static unsigned char value(uint64_t at, unsigned archive) {
    return (unsigned char)((at * 131) ^ (at >> 9) ^ (at >> 18) ^ (archive * 73));
}
static ssize_t backing(void *context, void *destination, size_t size, uint64_t offset) {
    struct Source *s = context; ++s->calls;
    if (s->interrupt) { s->interrupt = 0; errno = EINTR; return -1; }
    if (s->fail) { errno = EIO; return -1; }
    if (offset >= length) return 0;
    if (size > length - offset) size = length - offset;
    if (size > 4096) size = 4096; // exercise successful short backing reads
    for (size_t i = 0; i < size; ++i) ((unsigned char *)destination)[i] = value(offset + i, s->archive);
    return (ssize_t)size;
}
static void check(struct Source *source, uint64_t offset, size_t count) {
    unsigned char bytes[4096]; assert(count <= sizeof(bytes));
    size_t expected = offset >= length ? 0 : count;
    if (offset < length && expected > length - offset) expected = length - offset;
    assert(rio_archive_cache_read(&cache, source->archive, length, bytes, count,
        offset, backing, source) == (ssize_t)expected);
    for (size_t i = 0; i < expected; ++i) assert(bytes[i] == value(offset + i, source->archive));
}
static void *worker(void *argument) {
    struct Source s = {.archive = (unsigned)(uintptr_t)argument};
    for (unsigned i = 0; i < 100; ++i)
        check(&s, ((uint64_t)i * 77117 + s.archive * 113) % (8 * 1024 * 1024), 1000);
    return NULL;
}
static ssize_t blocked_backing(void *context, void *destination, size_t size, uint64_t offset) {
    pthread_mutex_lock(&gate_lock);
    entered = 1;
    pthread_cond_broadcast(&gate_changed);
    while (!unblock) pthread_cond_wait(&gate_changed, &gate_lock);
    pthread_mutex_unlock(&gate_lock);
    return backing(context, destination, size, offset);
}
static void *blocked_worker(void *unused) {
    (void)unused;
    struct Source s = {0}; unsigned char byte;
    assert(rio_archive_cache_read(&cache, 0, length, &byte, 1,
        (RIO_CACHE_BLOCKS + 300) * RIO_CACHE_BLOCK_SIZE, blocked_backing, &s) == 1);
    assert(byte == value((RIO_CACHE_BLOCKS + 300) * RIO_CACHE_BLOCK_SIZE, 0));
    return NULL;
}
static void *probe_worker(void *unused) {
    (void)unused;
    struct Source s = {0};
    check(&s, (RIO_CACHE_BLOCKS + 301) * RIO_CACHE_BLOCK_SIZE, 1);
    pthread_mutex_lock(&gate_lock);
    probe_done = 1; pthread_cond_broadcast(&gate_changed);
    pthread_mutex_unlock(&gate_lock);
    return NULL;
}
int main(void) {
    uint64_t first, end;
    assert(rio_prefetch_asset_bounds(17, 2796344, 3000000, RIO_CACHE_BLOCK_SIZE, &first, &end));
    assert(first == 0 && end == 2796361);
    unsigned blocks = 1;
    while (rio_prefetch_advance(&first, end, RIO_CACHE_BLOCK_SIZE)) ++blocks;
    assert(blocks == (2796361 + RIO_CACHE_BLOCK_SIZE - 1) / RIO_CACHE_BLOCK_SIZE && first < end && end - first <= RIO_CACHE_BLOCK_SIZE);
    assert(!rio_prefetch_asset_bounds(0, 1, 3000000, RIO_CACHE_BLOCK_SIZE, &first, &end));
    assert(!rio_prefetch_asset_bounds(0, 4194305, 5000000, RIO_CACHE_BLOCK_SIZE, &first, &end));
    assert(!rio_prefetch_asset_bounds(UINT64_MAX - 1, 262144, UINT64_MAX, RIO_CACHE_BLOCK_SIZE, &first, &end));
    assert(!rio_prefetch_asset_bounds(0, 262144, 3000000, 0, &first, &end));
    assert(rio_prefetch_asset_bounds(3000000 - 262144, 262144, 3000000, RIO_CACHE_BLOCK_SIZE, &first, &end));
    RioPrefetchPolicy prediction;
    rio_prefetch_reset(&prediction);
    uint64_t target;
    for (unsigned i = 0; i < 3; ++i)
        assert(!rio_prefetch_next(&prediction, i * 16384, 16384, length,
            RIO_CACHE_BLOCK_SIZE, &target));
    assert(rio_prefetch_next(&prediction, 49152, 16384, length, RIO_CACHE_BLOCK_SIZE, &target));
    assert(target == (65536 + RIO_CACHE_BLOCK_SIZE - 1) / RIO_CACHE_BLOCK_SIZE * RIO_CACHE_BLOCK_SIZE);
    assert(rio_prefetch_next(&prediction, 65536, 16384, length, RIO_CACHE_BLOCK_SIZE, &target) == (RIO_CACHE_BLOCK_SIZE <= 65536));
    assert(!rio_prefetch_next(&prediction, 500000, 16384, length, RIO_CACHE_BLOCK_SIZE, &target));
    assert(!rio_prefetch_next(&prediction, length - 65536, 65536, length,
        RIO_CACHE_BLOCK_SIZE, &target)); // never read beyond EOF
    assert(!rio_prefetch_next(&prediction, length - 1, 2, length, RIO_CACHE_BLOCK_SIZE, &target));
    assert(!rio_prefetch_next(&prediction, 0, 0, length, RIO_CACHE_BLOCK_SIZE, &target));
    assert(rio_prefetch_next(&prediction, 0, 65536, length, RIO_CACHE_BLOCK_SIZE, &target));
    assert(!rio_prefetch_next(&prediction, 400000, 16384, length, RIO_CACHE_BLOCK_SIZE, &target));
    assert(rio_prefetch_next(&prediction, 0, 65536, length, RIO_CACHE_BLOCK_SIZE, &target));
    assert(target == (65536 + RIO_CACHE_BLOCK_SIZE - 1) / RIO_CACHE_BLOCK_SIZE * RIO_CACHE_BLOCK_SIZE); // revisit may schedule a formerly evicted block
    void *small=malloc(RIO_CACHE_BLOCK_SIZE);assert(small);
    assert(rio_archive_cache_init_blocks(&cache,small,0)==EINVAL);
    assert(rio_archive_cache_init_blocks(&cache,small,RIO_CACHE_BLOCKS+1)==EINVAL);
    assert(!rio_archive_cache_init_blocks(&cache,small,1));
    struct Source limited={0};
    check(&limited,0,1);check(&limited,RIO_CACHE_BLOCK_SIZE,1);check(&limited,0,1);
    assert(cache.block_count==1 && rio_archive_cache_stats(&cache).fills==3);
    pthread_cond_destroy(&cache.changed);pthread_mutex_destroy(&cache.lock);free(small);
    void *memory = malloc(RIO_CACHE_BLOCKS * RIO_CACHE_BLOCK_SIZE); assert(memory);
    assert(!rio_archive_cache_init(&cache, memory));
    struct Source a = {0}, b = {.archive = 1}, reopened = {0};
    check(&a, RIO_CACHE_BLOCK_SIZE - 17, 93);
    RioArchiveStats stats = rio_archive_cache_stats(&cache);
    assert(stats.fills == 2 && stats.miss_bytes == 93);
    check(&reopened, RIO_CACHE_BLOCK_SIZE - 17, 93);
    assert(reopened.calls == 0); // independent handles reuse the archive blocks
    check(&b, RIO_CACHE_BLOCK_SIZE - 17, 93);
    assert(b.calls > 0); // main/patch cannot alias at the same offset
    check(&a, length - 10, 30); check(&a, length, 10);
    unsigned char byte;
    a.fail = 1;
    assert(rio_archive_cache_read(&cache, 0, length, &byte, 1,
        100 * RIO_CACHE_BLOCK_SIZE, backing, &a) == -1);
    a.fail = 0; a.interrupt = 1;
    check(&a, 100 * RIO_CACHE_BLOCK_SIZE, 1); // failed fill was not published
    for (unsigned i = 0; i <= RIO_CACHE_BLOCKS; ++i) check(&a, i * RIO_CACHE_BLOCK_SIZE, 1);
    unsigned before = a.calls;
    check(&a, 0, 1); assert(a.calls > before); // oldest block evicted
    pthread_t workers[4];
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_create(&workers[i], NULL, worker, (void *)(uintptr_t)(i & 1)));
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_join(workers[i], NULL));
    // A blocked disk fill must not block an independent cold block's fill.
    pthread_t blocked, probe;
    assert(!pthread_create(&blocked, NULL, blocked_worker, NULL));
    pthread_mutex_lock(&gate_lock);
    while (!entered) pthread_cond_wait(&gate_changed, &gate_lock);
    pthread_mutex_unlock(&gate_lock);
    assert(!pthread_create(&probe, NULL, probe_worker, NULL));
    struct timespec deadline; clock_gettime(CLOCK_REALTIME, &deadline); deadline.tv_sec += 2;
    pthread_mutex_lock(&gate_lock);
    while (!probe_done)
        if (pthread_cond_timedwait(&gate_changed, &gate_lock, &deadline) == ETIMEDOUT) break;
    int independent = probe_done;
    unblock = 1; pthread_cond_broadcast(&gate_changed);
    pthread_mutex_unlock(&gate_lock);
    assert(!pthread_join(blocked, NULL)); assert(!pthread_join(probe, NULL));
    assert(independent);
    assert(rio_archive_cache_read(&cache, 0, length, NULL, 1, 0, backing, &a) == -1 && errno == EFAULT);
    pthread_cond_destroy(&cache.changed);
    pthread_mutex_destroy(&cache.lock); free(memory);
    puts("Archive cache: byte accuracy, boundaries, shared handles, archive isolation, EOF, short reads, EINTR, failed fills, LRU and concurrency passed");
}
