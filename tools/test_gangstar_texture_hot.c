/* Texture reload cache: second-open admission, byte-exact copies, LRU bound,
 * slot bound, heap watermark refusal/shrink, eligibility, and the reader
 * handing an owned buffer to the engine on every path. */
#define _GNU_SOURCE
#include "gangstar_texture_hot.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ---- policy ---------------------------------------------------------- */
static void *filled(uint32_t size, unsigned char value) { void *p = malloc(size); memset(p, value, size); return p; }
static void policy(void) {
    static RioHotCache c;
    RioHotKey a = {0, 1000, 174904}, b = {0, 2000, 174904}, other_archive = {1, 1000, 174904};
    assert(!rio_hot_eligible(2872) && !rio_hot_eligible(2796344) && rio_hot_eligible(174904));
    assert(!rio_hot_note_open(&c, a));                     // first open only records
    assert(rio_hot_note_open(&c, a));                      // second open admits
    assert(!rio_hot_note_open(&c, other_archive));         // archive is part of the key
    void *d = filled(a.size, 7);
    assert(rio_hot_store(&c, a, d, 100u << 20) && c.count == 1 && c.bytes == a.size);
    RioHotEntry *e = rio_hot_find(&c, a); assert(e && e->data == d);
    assert(!rio_hot_find(&c, b) && !rio_hot_find(&c, other_archive));
    void *dup = filled(a.size, 1);
    assert(!rio_hot_store(&c, a, dup, 100u << 20)); free(dup);   // already cached: caller keeps buffer
    void *small = filled(4096, 1);
    assert(!rio_hot_store(&c, (RioHotKey){0, 9, 4096}, small, 0)); free(small);
    // Watermark: refused, and existing entries are released for the game.
    void *w = filled(b.size, 2);
    assert(!rio_hot_store(&c, b, w, RIO_HOT_HEAP_WATERMARK) && c.count == 0 && c.bytes == 0);
    free(w);
    // Byte bound with LRU: fill past the limit; the oldest goes first.
    unsigned capacity = RIO_HOT_LIMIT / 174904;
    for (unsigned i = 0; i < capacity + 5; ++i) {
        RioHotKey k = {0, 100000u + i * 200000u, 174904};
        if (i == capacity) assert(rio_hot_find(&c, (RioHotKey){0, 100000u, 174904}));   // touch #0 before evictions
        assert(rio_hot_store(&c, k, filled(k.size, (unsigned char)i), 50u << 20));
        assert(c.bytes <= RIO_HOT_LIMIT && c.count <= RIO_HOT_SLOTS);
    }
    assert(c.count == capacity);
    assert(rio_hot_find(&c, (RioHotKey){0, 100000u, 174904}));              // recently used survived
    assert(!rio_hot_find(&c, (RioHotKey){0, 100000u + 200000u, 174904}));   // oldest untouched evicted
    // Trim under memory pressure frees until under the watermark.
    rio_hot_trim(&c, RIO_HOT_LIMIT, RIO_HOT_HEAP_WATERMARK + 3u * 174904u);
    assert(c.count == capacity - 3);                 // frees exactly the overshoot
    rio_hot_trim(&c, 0, 0); assert(!c.count && !c.bytes);
    // Slot bound with tiny entries.
    for (unsigned i = 0; i < RIO_HOT_SLOTS + 10; ++i) {
        RioHotKey k = {0, 5000000u + i * 70000u, RIO_HOT_MIN_BYTES};
        assert(rio_hot_store(&c, k, filled(k.size, 3), 0));
        assert(c.count <= RIO_HOT_SLOTS);
    }
    assert(c.count == RIO_HOT_SLOTS);
    rio_hot_trim(&c, 0, 0);
    // Seen ring wraps without false admission.
    static RioHotCache r;
    for (unsigned i = 0; i < RIO_HOT_SEEN + 1; ++i) assert(!rio_hot_note_open(&r, (RioHotKey){0, i * 8u, 174904}));
    assert(!rio_hot_note_open(&r, (RioHotKey){0, 0, 174904}));             // oldest record overwritten
}

/* ---- reader against a fake archive ----------------------------------- */
#define ARCHIVE_BYTES (4u * 1024u * 1024u)
static unsigned char archive_bytes[ARCHIVE_BYTES];
static int fake_fd = 77, reads, fail_reads;
static size_t fake_heap;
int __wrap_open(const char *path, int flags, ...) { (void)flags; return strstr(path, "main.") ? fake_fd : -1; }
int __wrap_fstat(int fd, struct stat *st) { assert(fd == fake_fd); memset(st, 0, sizeof(*st)); st->st_size = 2119380688; return 0; }
int __wrap_close(int fd) { (void)fd; return 0; }
size_t rio_hot_test_heap_used(void) { return fake_heap; }
void gangstar_archive_track(int fd, const char *path, int flags) { (void)fd; (void)path; (void)flags; }
ssize_t gangstar_archive_pread(int fd, void *buffer, size_t size, off_t offset) {
    assert(fd == fake_fd); ++reads;
    if (fail_reads) { errno = EIO; return -1; }
    size_t take = size > 65536 ? 65536 : size;                // short reads are looped
    memcpy(buffer, archive_bytes + offset, take); return (ssize_t)take;
}
void _log_print(int kind, const char *fmt, ...) { (void)kind; (void)fmt; }
void *gangstar_texture_hot_get(int archive, uint64_t offset, uint32_t size);
static void reader(void) {
    for (unsigned i = 0; i < ARCHIVE_BYTES; ++i) archive_bytes[i] = (unsigned char)(i * 131u + 7u);
    const uint32_t size = 174904; const uint64_t at = 300001;
    fake_heap = 100u << 20;
    assert(!gangstar_texture_hot_get(0, at, size) && reads == 0);     // first open: native path
    unsigned char *one = gangstar_texture_hot_get(0, at, size);       // reload: read once, cache
    assert(one && reads == 3 && !memcmp(one, archive_bytes + at, size));
    unsigned char *two = gangstar_texture_hot_get(0, at, size);       // cached: no I/O, own copy
    assert(two && two != one && reads == 3 && !memcmp(two, archive_bytes + at, size));
    free(one); free(two);                                             // engine frees its copies
    assert(!gangstar_texture_hot_get(0, at, 2872));                   // ineligible size
    assert(!gangstar_texture_hot_get(2, at, size));                   // unknown archive
    // Loading-screen preload admits on the first open.
    void gangstar_texture_hot_preloading(int on);
    gangstar_texture_hot_preloading(1);
    int before_preload = reads;
    unsigned char *pre = gangstar_texture_hot_get(0, 2000003, size);
    gangstar_texture_hot_preloading(0);
    assert(pre && reads > before_preload && !memcmp(pre, archive_bytes + 2000003, size)); free(pre);
    before_preload = reads;
    pre = gangstar_texture_hot_get(0, 2000003, size);
    assert(pre && reads == before_preload); free(pre);              // first gameplay open: RAM
    // I/O failure falls back to the native path.
    assert(!gangstar_texture_hot_get(0, 900000, size)); fail_reads = 1;
    assert(!gangstar_texture_hot_get(0, 900000, size)); fail_reads = 0;
    // Above the watermark: reloads are not read here, and the cache empties.
    fake_heap = RIO_HOT_HEAP_WATERMARK + 1;
    assert(!gangstar_texture_hot_get(0, 1500000, size));
    assert(!gangstar_texture_hot_get(0, 1500000, size));
    int before = reads;
    assert(!gangstar_texture_hot_get(0, at, size) && reads == before);   // previously cached entry released
}
int main(void) {
    policy(); reader();
    puts("Texture reload cache: second-open admission, exact copies, LRU/slot/byte bounds, heap watermark and native fallbacks passed");
}
