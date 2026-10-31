#include "gangstar_texture_lod.h"
#include "gangstar_archive_cache.h"
#include "gangstar_source_cache.h"
#include "utils/logger.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#if defined(__vita__)
#include <malloc.h>
#include <psp2/kernel/threadmgr.h>
#endif
#ifdef GANGSTAR_SMALL_LIGHTMAPS
// Each generated buffer is transferred to the native memory file. Its native
// reference-counted destructor frees the buffer when the reader is released.
extern void gangstar_host_heap_allocated(void *,size_t,uintptr_t) __attribute__((weak));
static atomic_uint made, rejected, source_hits, source_misses, original_streamed;
static int source_preloading;
static _Atomic uint64_t source_hit_bytes;
static RioSourceCache source_cache;
static pthread_mutex_t source_lock=PTHREAD_MUTEX_INITIALIZER;
static RioTextureSourcePriority source_priorities[384];
static unsigned source_priority_count;
static unsigned source_priority(RioSourceKey key) {
 for(unsigned i=0;i<source_priority_count;++i)
  if(source_priorities[i].offset==key.offset && source_priorities[i].size==key.source_size && source_priorities[i].original==key.original)return source_priorities[i].priority;
 return 0;
}
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
static _Atomic uint64_t saved, original_hit_bytes;
static atomic_uint original_hits;
// Worker and game reads must not share a per-descriptor cursor lock.
static int source_fds[2][2]={{-1,-1},{-1,-1}};
static const char *paths[] = {
    "ux0:/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb",
    "ux0:/data/gangstarrio/patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb"
};
static const uint64_t lengths[] = {UINT64_C(2119380688), UINT64_C(3709754)};
static int read_exact(int fd, void *data, size_t length, uint64_t offset,int background) {
    size_t done = 0;
    while (done < length) {
#ifdef __vita__
        // Outside any cache/descriptor lock: foreground/audio reads take priority.
        while(background && gangstar_archive_demand_busy())sceKernelDelayThread(1000);
#endif
        size_t take=length-done;if(background && take>64u*1024u)take=64u*1024u;
        ssize_t n = gangstar_archive_pread(fd, (char *)data + done, take, offset + done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        done += n;
#ifdef __vita__
        if(background && done<length)sceKernelDelayThread(1000);
#endif
    }
    return 1;
}
#endif
static const void *texture_source_get(int archive, uint64_t offset, uint32_t size,
    const char *name, uint32_t *output_size, int original, int background) {
#ifdef GANGSTAR_SMALL_LIGHTMAPS
    if (!output_size || (original && (archive!=0 || !rio_lod_original_source(name))) ||
        archive < 0 || archive > 1 || !rio_lod_name(name) || (size != 2796344 && size != 699192 && size != 349672) ||
        offset >= lengths[archive] || size > lengths[archive] - offset) return NULL;
    pthread_mutex_lock(&source_lock);
    size_t cache_limit=source_budget(size), cached_size=0;
    RioSourceKey key={archive,original,offset,size};
    if(background && (!source_priority(key) || rio_source_find(&source_cache,key))) {pthread_mutex_unlock(&source_lock);return NULL;}
    void *cached=rio_source_clone(&source_cache,key,&cached_size);
    if(cached) {
        ++source_hits;source_hit_bytes+=cached_size;++made;saved+=size-cached_size;
        if(original){++original_hits;original_hit_bytes+=size;}
        if(!background && gangstar_host_heap_allocated)gangstar_host_heap_allocated(cached,cached_size,(uintptr_t)0xfffffff0u);
        *output_size=(uint32_t)cached_size;pthread_mutex_unlock(&source_lock);return cached;
    }
    if(rio_source_find(&source_cache,key)) {rio_source_trim(&source_cache,0);cache_limit=0;}
    ++source_misses;
    // A native miss reads the whole lightmap anyway. Retain that first read
    // only in spare cache space; never displace the fixed startup preload.
    int spare_fill=original && !source_preloading && !background;
    if(spare_fill && (source_cache.count==RIO_SOURCE_CACHE_SLOTS ||
        source_cache.bytes>cache_limit || size>cache_limit-source_cache.bytes)) {
        ++original_streamed;pthread_mutex_unlock(&source_lock);return NULL;
    }
    int *fds=source_fds[background!=0];
    if (fds[archive] < 0) {
        int fd = open(paths[archive], O_RDONLY); struct stat st;
        if (fd < 0) { ++rejected; pthread_mutex_unlock(&source_lock);return NULL; }
        if (fstat(fd, &st) || (uint64_t)st.st_size != lengths[archive]) {
            close(fd); ++rejected;pthread_mutex_unlock(&source_lock);return NULL;
        }
        gangstar_archive_track(fd, paths[archive], O_RDONLY);
        fds[archive] = fd;
    }
    pthread_mutex_unlock(&source_lock);
    unsigned char header[128]; RioTextureLod plan;
    if (!read_exact(fds[archive], header, sizeof(header), offset,background) || !rio_lod_plan(header, size, &plan)) {
        ++rejected; return NULL;
    }
    if (original) {
        // Validate the original DDS first, then retain every byte unchanged.
        plan.skip=0;plan.output_size=size;memcpy(plan.header,header,128);
    }
    if (!plan.output_size || plan.output_size > size) { ++rejected; return NULL; }
    void *data = malloc(plan.output_size);
    if(!data) {
        pthread_mutex_lock(&source_lock);rio_source_trim(&source_cache,0);pthread_mutex_unlock(&source_lock);
        cache_limit=0;data=malloc(plan.output_size);
    }
    if (!data) { ++rejected; return NULL; }
    memcpy(data, plan.header, 128);
    if (!read_exact(fds[archive], (char *)data + 128, plan.output_size - 128, offset + 128 + plan.skip,background)) {
        free(data); ++rejected; return NULL;
    }
    pthread_mutex_lock(&source_lock);
    cache_limit=source_budget(0);
    unsigned priority=source_priority(key);
    // Do not evict a current-district entry to cache obsolete background work.
    size_t available=cache_limit>source_cache.bytes?cache_limit-source_cache.bytes:0;
    for(unsigned i=0;i<RIO_SOURCE_CACHE_SLOTS;++i)
        if(source_cache.entries[i].data && source_cache.entries[i].priority<=priority)available+=source_cache.entries[i].size;
    if(spare_fill)available=cache_limit>source_cache.bytes?cache_limit-source_cache.bytes:0;
    if(available>=plan.output_size && (!spare_fill || source_cache.count<RIO_SOURCE_CACHE_SLOTS))rio_source_store(&source_cache,key,data,plan.output_size,cache_limit);
    RioSourceEntry *retained=rio_source_find(&source_cache,key);if(retained)retained->priority=priority;
    pthread_mutex_unlock(&source_lock);
    // Native destruction calls the imported free, which removes this entry.
    if (!background && gangstar_host_heap_allocated)
        gangstar_host_heap_allocated(data,plan.output_size,(uintptr_t)0xfffffff0u);
    ++made; saved += plan.skip;
    if (original && !background) { ++original_hits; original_hit_bytes += size; }
    l_info("RIO_LOD_CREATE name=%s original=%d source_bytes=%u reduced_bytes=%u width=%u height=%u", name, original, size, plan.output_size, rio_lod_u32(plan.header+16), rio_lod_u32(plan.header+12));
    *output_size = plan.output_size;
    return data;
#else
    (void)archive; (void)offset; (void)size; (void)name; (void)output_size; (void)original; (void)background;return NULL;
#endif
}
void gangstar_texture_source_preloading(int active) {
#ifdef GANGSTAR_SMALL_LIGHTMAPS
    pthread_mutex_lock(&source_lock);source_preloading=active!=0;pthread_mutex_unlock(&source_lock);
#else
    (void)active;
#endif
}
const void *gangstar_texture_lod_get(int archive,uint64_t offset,uint32_t size,
    const char *name,uint32_t *output_size) {
    return texture_source_get(archive,offset,size,name,output_size,0,0);
}
const void *gangstar_texture_original_get(int archive,uint64_t offset,uint32_t size,
    const char *name,uint32_t *output_size) {
    return texture_source_get(archive,offset,size,name,output_size,1,0);
}
void gangstar_texture_source_priorities(const RioTextureSourcePriority *priorities,unsigned count) {
#ifdef GANGSTAR_SMALL_LIGHTMAPS
 if(count>384)return;
 pthread_mutex_lock(&source_lock);
 source_priority_count=count;
 if(count)memcpy(source_priorities,priorities,count*sizeof(*priorities));
 for(unsigned i=0;i<RIO_SOURCE_CACHE_SLOTS;++i)if(source_cache.entries[i].data)
  source_cache.entries[i].priority=source_priority(source_cache.entries[i].key);
 pthread_mutex_unlock(&source_lock);
#else
 (void)priorities;(void)count;
#endif
}
int gangstar_texture_source_background(uint64_t offset,uint32_t size,const char *name) {
 uint32_t length=0;
 void *data=(void *)texture_source_get(0,offset,size,name,&length,rio_lod_original_source(name),1);
 free(data);
#ifdef GANGSTAR_SMALL_LIGHTMAPS
 RioSourceKey key={0,rio_lod_original_source(name),offset,size};
 pthread_mutex_lock(&source_lock);int retained=rio_source_find(&source_cache,key)!=NULL;pthread_mutex_unlock(&source_lock);
 return retained;
#else
 return 0;
#endif
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
    pthread_mutex_lock(&source_lock);
    l_info("RIO_SOURCE_CACHE hits=%u misses=%u avoided_read_bytes=%llu entries=%u bytes=%u limit=%u evictions=%u",
        source_hits,source_misses,(unsigned long long)source_hit_bytes,source_cache.count,
        (unsigned)source_cache.bytes,RIO_SOURCE_CACHE_LIMIT,source_cache.evictions);
    source_hits=source_misses=source_cache.evictions=0;source_hit_bytes=0;
    pthread_mutex_unlock(&source_lock);
    made = rejected = 0; saved = 0;
#else
    (void)frame;
#endif
}
