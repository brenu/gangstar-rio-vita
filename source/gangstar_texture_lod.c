#include "gangstar_texture_lod.h"
#include "gangstar_archive_cache.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#if defined(__vita__) && defined(DEBUG_SOLOADER)
#include <malloc.h>
#endif
#ifdef GANGSTAR_SMALL_LIGHTMAPS
// Main-thread-only publication; buffers remain immutable and valid until exit.
// Native memory files borrow them, so never evict/free a published allocation.
static struct { int archive, original; uint64_t offset; uint32_t source_size, size; void *data; } retained[RIO_LOD_SLOTS];
static unsigned count, bytes, hits, made, rejected;
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
    for (unsigned i = 0; i < count; ++i)
        if (retained[i].archive == archive && retained[i].offset == offset && retained[i].source_size == size && retained[i].original == original) {
            ++hits; saved += size-retained[i].size;
            if (original) { ++original_hits; original_hit_bytes+=size; }
            *output_size = retained[i].size; return retained[i].data;
        }
    if (count == RIO_LOD_SLOTS) { ++rejected; return NULL; }
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
    if (plan.output_size > RIO_LOD_RAM_LIMIT - bytes) { ++rejected; return NULL; }
    void *data = malloc(plan.output_size);
    if (!data) { ++rejected; return NULL; }
    memcpy(data, plan.header, 128);
    if (!read_exact(fds[archive], (char *)data + 128, plan.output_size - 128, offset + 128 + plan.skip)) {
        free(data); ++rejected; return NULL;
    }
    retained[count].original=original;
    retained[count].archive = archive; retained[count].offset = offset;
    retained[count].source_size = size; retained[count].size = plan.output_size;
    retained[count++].data = data;
    bytes += plan.output_size; ++made; saved += plan.skip;
    l_info("RIO_LOD_CREATE name=%s original=%d source_bytes=%u reduced_bytes=%u width=%u height=%u", name, original, size, plan.output_size, rio_lod_u32(plan.header+16), rio_lod_u32(plan.header+12));
    *output_size = plan.output_size;
    return data;
#else
    (void)archive; (void)offset; (void)size; (void)name; (void)output_size; (void)original; return NULL;
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
    l_info("RIO_LOD frame=%u retained=%u ram_bytes=%u created=%u hits=%u fallback=%u logical_reduction_bytes=%llu",
        frame, count, bytes, made, hits, rejected, (unsigned long long)saved);
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
    original_hits=0;original_hit_bytes=0;
    made = hits = rejected = 0; saved = 0;
#else
    (void)frame;
#endif
}
