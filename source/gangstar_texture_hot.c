/* Reload cache for plain archive textures (policy in gangstar_texture_hot.h).
 * Frame thread only: the caller (pak-entry open hook) checks the thread. */
#include "gangstar_texture_hot.h"
#include "gangstar_archive_cache.h"
#include "utils/logger.h"
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>

static RioHotCache hot;
static int hot_preloading;   // loading screen: admit on first open
void gangstar_texture_hot_preloading(int on) { hot_preloading = on; }
static int hot_fds[2] = {-1, -1};
static const char *const hot_paths[2] = {
    "ux0:/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb",
    "ux0:/data/gangstarrio/patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb"
};
static const uint64_t hot_lengths[2] = {UINT64_C(2119380688), UINT64_C(3709754)};

#ifdef __vita__
static size_t heap_used(void) { struct mallinfo m = mallinfo(); return (size_t)m.uordblks; }
#else
size_t rio_hot_test_heap_used(void);   // host tests supply the heap reading
static size_t heap_used(void) { return rio_hot_test_heap_used(); }
#endif

static int hot_fd(int archive) {
    if (hot_fds[archive] == -2) return -1;
    if (hot_fds[archive] < 0) {
        int fd = open(hot_paths[archive], O_RDONLY); struct stat st;
        if (fd < 0 || fstat(fd, &st) || (uint64_t)st.st_size != hot_lengths[archive]) {
            if (fd >= 0) close(fd);
            hot_fds[archive] = -2; return -1;
        }
        gangstar_archive_track(fd, hot_paths[archive], O_RDONLY);
        hot_fds[archive] = fd;
    }
    return hot_fds[archive];
}

// One positional read of the whole entry (batched by the archive cache).
static void *read_entry(int archive, uint64_t offset, uint32_t size) {
    int fd = hot_fd(archive);
    if (fd < 0 || offset > hot_lengths[archive] || size > hot_lengths[archive] - offset) return NULL;
    unsigned char *data = malloc(size);
    if (!data) return NULL;
    size_t done = 0;
    while (done < size) {
        ssize_t n = gangstar_archive_pread(fd, data + done, size - done, (off_t)(offset + done));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { free(data); return NULL; }
        done += (size_t)n;
    }
    return data;
}

// Returns an owned copy of the entry for the engine, or NULL to use the
// normal archive path (first open, ineligible, memory pressure or I/O error).
void *gangstar_texture_hot_get(int archive, uint64_t offset, uint32_t size) {
    if (archive < 0 || archive > 1 || !rio_hot_eligible(size)) return NULL;
    RioHotKey key = {archive, offset, size};
    size_t used = heap_used();
    rio_hot_trim(&hot, RIO_HOT_LIMIT, used);
    RioHotEntry *entry = rio_hot_find(&hot, key);
    if (entry) {
        void *copy = malloc(size);
        if (!copy) return NULL;
        memcpy(copy, entry->data, size); ++hot.hits;
        return copy;
    }
    if (!rio_hot_note_open(&hot, key) && !hot_preloading) return NULL;   // first open: native path
    if (used > RIO_HOT_HEAP_WATERMARK - size) { ++hot.refused; return NULL; }
    void *data = read_entry(archive, offset, size);
    if (!data) return NULL;
    void *copy = malloc(size);
    if (copy && rio_hot_store(&hot, key, data, used + size)) {
        memcpy(copy, data, size);
        return copy;                                      // cache keeps `data`
    }
    free(copy);
    return data;                                          // not retained: hand over the read
}

void gangstar_texture_hot_report(void) {
    l_info("RIO_TEXTURE_HOT entries=%u bytes=%u hits=%u admitted=%u evicted=%u refused=%u heap_used=%u",
        hot.count, (unsigned)hot.bytes, hot.hits, hot.admitted, hot.evicted, hot.refused, (unsigned)heap_used());
    hot.hits = hot.admitted = hot.evicted = hot.refused = 0;
}
