#include "gangstar_texture_lod.h"
#include "gangstar_archive_cache.h"
#include "gangstar_source_cache.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#if defined(__vita__)
#include <malloc.h>
#endif
#ifdef GANGSTAR_SMALL_LIGHTMAPS
// Each generated buffer is transferred to the native memory file. Its native
// reference-counted destructor frees the buffer when the reader is released.
extern void gangstar_host_heap_allocated(void *,size_t,uintptr_t) __attribute__((weak));
static unsigned made, rejected, source_hits, source_misses, original_streamed;
static int source_preloading;
static uint64_t source_hit_bytes;
static RioSourceCache source_cache;
static size_t source_budget(size_t reader_bytes) {
    size_t limit=RIO_SOURCE_CACHE_LIMIT;
#ifdef __vita__
    struct mallinfo heap=mallinfo();
    limit=rio_source_allowance(limit,source_cache.bytes,(size_t)heap.uordblks,192u*1024u*1024u,reader_bytes);
#else
    (void)reader_bytes;
#endif
    rio_source_trim(&source_cache,limit);return limit;
}
static uint64_t saved, original_hit_bytes;
static unsigned original_hits;
static int fds[2] = {-1, -1};
static const char *paths[] = {
    "ux0:/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb",
    "ux0:/data/gangstarrio/patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb"
};
static const uint64_t lengths[] = {UINT64_C(2119380688), UINT64_C(3709754)};
static int read_exact(int fd, void *data, size_t length, uint64_t offset) {
    size_t done = 0;
    while (done < length) {
        ssize_t n = gangstar_archive_pread(fd, (char *)data + done, length - done, offset + done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        done += n;
    }
    return 1;
}
#endif
static const void *texture_source_get(int archive, uint64_t offset, uint32_t size,
    const char *name, uint32_t *output_size, int original) {
#ifdef GANGSTAR_SMALL_LIGHTMAPS
    if (!output_size || (original && (archive!=0 || !rio_lod_original_source(name))) ||
        archive < 0 || archive > 1 || !rio_lod_name(name) || (size != 2796344 && size != 699192 && size != 349672) ||
        offset >= lengths[archive] || size > lengths[archive] - offset) return NULL;
    size_t cache_limit=source_budget(size), cached_size=0;
    RioSourceKey key={archive,original,offset,size};
    void *cached=rio_source_clone(&source_cache,key,&cached_size);
    if(cached) {
        ++source_hits;source_hit_bytes+=cached_size;++made;saved+=size-cached_size;
        if(original){++original_hits;original_hit_bytes+=size;}
        if(gangstar_host_heap_allocated)gangstar_host_heap_allocated(cached,cached_size,(uintptr_t)0xfffffff0u);
        *output_size=(uint32_t)cached_size;return cached;
    }
    if(rio_source_find(&source_cache,key)) {rio_source_trim(&source_cache,0);cache_limit=0;}
    ++source_misses;
    // A full original lightmap is ~2.8 MiB, while the native streaming reader
    // may need only a low mip. Never turn that small read into a full-file
    // synchronous cache fill during gameplay. Preload fills are explicit.
    if(original && !source_preloading) {++original_streamed;return NULL;}
    if (fds[archive] < 0) {
        int fd = open(paths[archive], O_RDONLY); struct stat st;
        if (fd < 0) { ++rejected; return NULL; }
        if (fstat(fd, &st) || (uint64_t)st.st_size != lengths[archive]) {
            close(fd); ++rejected; return NULL;
        }
        gangstar_archive_track(fd, paths[archive], O_RDONLY);
        fds[archive] = fd;
    }
    unsigned char header[128]; RioTextureLod plan;
    if (!read_exact(fds[archive], header, sizeof(header), offset) || !rio_lod_plan(header, size, &plan)) {
        ++rejected; return NULL;
    }
    if (original) {
        // Validate the original DDS first, then retain every byte unchanged.
        plan.skip=0;plan.output_size=size;memcpy(plan.header,header,128);
    }
    if (!plan.output_size || plan.output_size > size) { ++rejected; return NULL; }
    void *data = malloc(plan.output_size);
    if(!data && source_cache.bytes) {
        rio_source_trim(&source_cache,0);cache_limit=0;data=malloc(plan.output_size);
    }
    if (!data) { ++rejected; return NULL; }
    memcpy(data, plan.header, 128);
    if (!read_exact(fds[archive], (char *)data + 128, plan.output_size - 128, offset + 128 + plan.skip)) {
        free(data); ++rejected; return NULL;
    }
    rio_source_store(&source_cache,key,data,plan.output_size,cache_limit);
    // Native destruction calls the imported free, which removes this entry.
    if (gangstar_host_heap_allocated)
        gangstar_host_heap_allocated(data,plan.output_size,(uintptr_t)0xfffffff0u);
    ++made; saved += plan.skip;
    if (original) { ++original_hits; original_hit_bytes += size; }
    l_info("RIO_LOD_CREATE name=%s original=%d source_bytes=%u reduced_bytes=%u width=%u height=%u", name, original, size, plan.output_size, rio_lod_u32(plan.header+16), rio_lod_u32(plan.header+12));
    *output_size = plan.output_size;
    return data;
#else
    (void)archive; (void)offset; (void)size; (void)name; (void)output_size; (void)original; return NULL;
#endif
}
void gangstar_texture_source_preloading(int active) {
#ifdef GANGSTAR_SMALL_LIGHTMAPS
    source_preloading=active!=0;
#else
    (void)active;
#endif
}
const void *gangstar_texture_lod_get(int archive,uint64_t offset,uint32_t size,
    const char *name,uint32_t *output_size) {
    return texture_source_get(archive,offset,size,name,output_size,0);
}
const void *gangstar_texture_original_get(int archive,uint64_t offset,uint32_t size,
    const char *name,uint32_t *output_size) {
    return texture_source_get(archive,offset,size,name,output_size,1);
}
void gangstar_texture_lod_report(unsigned frame) {
    (void)frame;
#ifdef GANGSTAR_SMALL_LIGHTMAPS
    l_info("RIO_LOD frame=%u mode=owned_reader_buffers created=%u fallback=%u logical_reduction_bytes=%llu",
        frame, made, rejected, (unsigned long long)saved);
#if defined(__vita__) && defined(DEBUG_SOLOADER)
    if (made) {
        struct mallinfo heap = mallinfo();
        l_info("RIO_LOD_HEAP frame=%u arena=%d used=%d free=%d", frame, heap.arena, heap.uordblks, heap.fordblks);
    }
#endif
    if (original_hits) {
        l_info("RIO_LIGHTMAP_SOURCE hits=%u avoided_read_bytes=%llu",
            original_hits,(unsigned long long)original_hit_bytes);
    }
    l_info("RIO_LIGHTMAP_STREAM native_partial_read_fallbacks=%u",original_streamed);
    original_streamed=0;
    original_hits=0;original_hit_bytes=0;
    l_info("RIO_SOURCE_CACHE hits=%u misses=%u avoided_read_bytes=%llu entries=%u bytes=%u limit=%u evictions=%u",
        source_hits,source_misses,(unsigned long long)source_hit_bytes,source_cache.count,
        (unsigned)source_cache.bytes,RIO_SOURCE_CACHE_LIMIT,source_cache.evictions);
    source_hits=source_misses=source_cache.evictions=0;source_hit_bytes=0;
    made = rejected = 0; saved = 0;
#else
    (void)frame;
#endif
}
