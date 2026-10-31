#include "gangstar_audio_probe.h"
#include "gangstar_archive_cache.h"
#include "gangstar_prefetch_policy.h"
#include "utils/logger.h"
#include <psp2/kernel/sysmem.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>
#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <malloc.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#endif

static atomic_uint read_position_hits __attribute__((unused)),read_position_fallbacks;
static atomic_int trace_thread;
static unsigned trace_frame;
static GangstarArchiveFrame frame_stats;
GangstarArchiveFrame gangstar_archive_frame_stats(void) { return frame_stats; }
static uint64_t trace_clock(void) {
#ifdef __vita__
    return sceKernelGetProcessTimeWide();
#else
    return 0;
#endif
}
static int trace_main(void) {
#ifdef __vita__
    return atomic_load_explicit(&trace_thread, memory_order_relaxed) == sceKernelGetThreadId();
#else
    return 0;
#endif
}
void gangstar_archive_frame(unsigned frame) {
#ifdef __vita__
    atomic_store_explicit(&trace_thread, sceKernelGetThreadId(), memory_order_relaxed);
#else
    (void)trace_thread;
#endif
    trace_frame = frame;
    frame_stats = (GangstarArchiveFrame){0};
}
static int trace_audio(void) {
#ifdef __vita__
    return rio_audio_scope_active();
#else
    return 0;
#endif
}
static void trace_audio_phase(unsigned phase, uint64_t elapsed) {
#ifdef __vita__
    rio_audio_phase(phase, elapsed);
#else
    (void)phase; (void)elapsed;
#endif
}
struct BackingContext {
    int fd, tracing, archive, audio;
    uint64_t disk_bytes, disk_us, first_fill, last_fill;
    unsigned disk_calls;
};
static struct ReadTrace {
    unsigned frame;
    int archive;
    uint64_t offset, requested, returned, elapsed, disk_bytes, disk_us, first_fill, last_fill;
    unsigned disk_calls;
} traces[64];
static unsigned trace_count, trace_dropped;
// Main-thread-only ring; defer log writes until the normal reporting point.
static void trace_read(struct BackingContext *context, int archive, off_t offset,
    size_t requested, ssize_t returned, uint64_t started) {
    if (!context->tracing) return;
    uint64_t elapsed = trace_clock() - started;
    // Include every request, even those below the detailed trace cutoff.
    ++frame_stats.calls;
    frame_stats.requested += requested;
    frame_stats.returned += returned > 0 ? (uint64_t)returned : 0;
    frame_stats.elapsed_us += elapsed;
    frame_stats.disk_bytes += context->disk_bytes;
    frame_stats.disk_us += context->disk_us;
    frame_stats.disk_calls += context->disk_calls;
    if (elapsed < 10000) return;
    if (trace_count == 64) { ++trace_dropped; return; }
    traces[trace_count++] = (struct ReadTrace){trace_frame, archive, (uint64_t)offset,
        requested, returned > 0 ? (uint64_t)returned : 0, elapsed,
        context->disk_bytes, context->disk_us, context->first_fill, context->last_fill,
        context->disk_calls};
}

static RioArchiveCache cache;
static atomic_int enabled;
static atomic_uint active_demand;
int gangstar_archive_demand_busy(void) {return atomic_load(&active_demand)!=0;}
static void prefetch_init(void);
static void prefetch_enqueue(int archive, uint64_t offset);
static const uint64_t archive_sizes[] = {UINT64_C(2119380688), UINT64_C(3709754)};
static const char *archive_paths[] = {
    "ux0:/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb",
    "ux0:/data/gangstarrio/patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb"
};
#if defined(__vita__) && defined(GANGSTAR_NATIVE_ARCHIVE_IO)
static int native_fds[] = {-1, -1};
static atomic_uint native_calls, native_bytes, native_fallbacks;
static void native_backend_init(void) {
    for (unsigned archive = 0; archive < 2; ++archive) {
        int native = sceIoOpen(archive_paths[archive], SCE_O_RDONLY, 0);
        int original = open(archive_paths[archive], O_RDONLY);
        SceIoStat st;
        int valid = native >= 0 && original >= 0 &&
            sceIoGetstatByFd(native, &st) >= 0 && (uint64_t)st.st_size == archive_sizes[archive];
        const uint64_t offsets[] = {0, RIO_CACHE_BLOCK_SIZE - 17, archive_sizes[archive] - 64};
        unsigned char expected[64], actual[64];
        for (unsigned i = 0; valid && i < 3; ++i) {
            valid = pread(original, expected, sizeof(expected), offsets[i]) == sizeof(expected) &&
                sceIoPread(native, actual, sizeof(actual), offsets[i]) == sizeof(actual) &&
                !memcmp(expected, actual, sizeof(actual));
        }
        if (original >= 0) close(original);
        if (valid) native_fds[archive] = native; // process-lifetime positional handle
        else if (native >= 0) sceIoClose(native);
        l_info("Gangstar archive backend: archive=%u native=%d parity=%s", archive,
            native_fds[archive], valid ? "passed" : "failed; using newlib");
    }
}
#else
static void native_backend_init(void) { (void)archive_paths; }
#endif
// Vita newlib has 256 logical descriptors; many can share one kernel handle.
enum { MAX_HANDLES = 256 };
static int shared_roots[2]={-1,-1};
static atomic_uint shared_root_count,shared_readers,shared_peak;
static pthread_mutex_t root_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t physical_lock[2]={PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER};
static struct Handle {
    int fd, archive, closing, shared;
    unsigned references;
    FILE *stream;
    off_t known_offset;
    int offset_valid,exposed,dirty;
    RioPrefetchPolicy prediction;
    pthread_mutex_t position;
} handles[MAX_HANDLES];
static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t table_changed = PTHREAD_COND_INITIALIZER;
static atomic_uint virtual_advances __attribute__((unused)),cursor_syncs,cursor_sync_errors;
static atomic_uint position_hits,position_fallbacks,position_exposures;

void gangstar_archive_init(void) {
#ifdef GANGSTAR_DISABLE_ARCHIVE_CACHE
    l_info("Gangstar shared archive cache disabled");
    return;
#endif
    // Separate kernel allocation leaves the game's malloc arena budget intact.
    unsigned blocks=RIO_CACHE_BLOCKS;
#ifdef __vita__
    SceKernelFreeMemorySizeInfo before={.size=sizeof(before)};
    int memory_info=sceKernelGetFreeMemorySize(&before);
    /* Keep at least 32 MiB of reported user memory for later kernel/heap
       allocations. Allocation can still fail due to fragmentation. */
    if(memory_info<0)blocks=16*1024/RIO_CACHE_BLOCK_KIB;
    while(blocks>16*1024/RIO_CACHE_BLOCK_KIB && memory_info>=0 &&
        before.size_user<(int)(blocks*RIO_CACHE_BLOCK_SIZE+32*1024*1024))
        blocks/=2;
#endif
    void *memory=NULL;
    SceUID block=-1;
    for(;;) {
        block=sceKernelAllocMemBlock("Rio archive cache",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
            blocks*RIO_CACHE_BLOCK_SIZE,NULL);
        if(block>=0 && sceKernelGetMemBlockBase(block,&memory)>=0)break;
        if(block>=0)sceKernelFreeMemBlock(block);
        if(blocks>16*1024/RIO_CACHE_BLOCK_KIB) { blocks/=2;continue; }
        l_warn("Gangstar archive cache unavailable; using original I/O");return;
    }
    if (rio_archive_cache_init_blocks(&cache,memory,blocks)) {
        sceKernelFreeMemBlock(block); return;
    }
#ifdef __vita__
    SceKernelFreeMemorySizeInfo after={.size=sizeof(after)};
    int after_info=sceKernelGetFreeMemorySize(&after);
    l_info("RIO_CACHE_MEMORY requested_mib=%u actual_mib=%u user_free_before=%d user_free_after=%d",
        RIO_CACHE_MIB,(unsigned)(blocks*RIO_CACHE_BLOCK_SIZE/(1024*1024)),
        memory_info<0?-1:before.size_user,after_info<0?-1:after.size_user);
#endif
    unsigned initialized = 0;
    for (; initialized < MAX_HANDLES; ++initialized) {
        handles[initialized].fd = -1;
        if (pthread_mutex_init(&handles[initialized].position, NULL)) break;
    }
    if (initialized != MAX_HANDLES) {
        while (initialized) pthread_mutex_destroy(&handles[--initialized].position);
        pthread_cond_destroy(&cache.changed);
        pthread_mutex_destroy(&cache.lock); sceKernelFreeMemBlock(block); return;
    }
    native_backend_init();
    enabled = 1;
    l_info("Gangstar shared archive cache: %u MiB, %u x %u KiB, stdio and descriptor reads",
        (unsigned)(cache.block_count * RIO_CACHE_BLOCK_SIZE / (1024 * 1024)),
        cache.block_count, (unsigned)(RIO_CACHE_BLOCK_SIZE / 1024));
    prefetch_init();
}

void gangstar_archive_track(int fd, const char *path, int flags) {
    if (!enabled || fd < 0 || (flags & O_ACCMODE) != O_RDONLY || strncmp(path, "ux0:", 4)) return;
    path += 4;
    if (*path == '/') ++path;
    int archive = -1;
    if (!strcmp(path, "data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb")) archive = 0;
    if (!strcmp(path, "data/gangstarrio/patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb")) archive = 1;
    struct stat st;
    if (archive < 0 || fstat(fd, &st) || (uint64_t)st.st_size != archive_sizes[archive]) return;
    pthread_mutex_lock(&table_lock);
    for(unsigned i=0;i<MAX_HANDLES;++i)if(handles[i].fd==fd) {
        pthread_mutex_unlock(&table_lock);return;
    }
    for (unsigned i = 0; i < MAX_HANDLES; ++i) if (handles[i].fd == -1) {
        handles[i].archive = archive;
        handles[i].closing = 0;
        handles[i].shared = 0;
        handles[i].references = 0;
        handles[i].stream = NULL;
        handles[i].offset_valid=handles[i].exposed=handles[i].dirty=0;
        handles[i].known_offset=0;
        rio_prefetch_reset(&handles[i].prediction);
        handles[i].fd = fd;
        break; // table full: retain normal I/O for that descriptor
    }
    pthread_mutex_unlock(&table_lock);
}

static struct Handle *acquire(int fd) {
    if (!enabled || fd < 0) return NULL;
    struct Handle *h = NULL;
    pthread_mutex_lock(&table_lock);
    for (unsigned i = 0; i < MAX_HANDLES; ++i)
        if (handles[i].fd == fd && !handles[i].closing) {
            h = &handles[i]; ++h->references; break;
        }
    pthread_mutex_unlock(&table_lock);
    if (h) pthread_mutex_lock(&h->position);
    return h;
}

static void release(struct Handle *h) {
    pthread_mutex_unlock(&h->position);
    pthread_mutex_lock(&table_lock);
    if (!--h->references && h->closing) pthread_cond_broadcast(&table_changed);
    pthread_mutex_unlock(&table_lock);
}

int gangstar_archive_open_shared(const char *path) {
    if(!enabled)return -2;
    int archive=-1;
    for(unsigned i=0;i<2;++i)if(!strcmp(path,archive_paths[i]))archive=(int)i;
    if(archive<0)return -2;
    pthread_mutex_lock(&root_lock);
    if(shared_roots[archive]<0) {
        int root=open(path,O_RDONLY);struct stat st;
        if(root>=0 && (!fstat(root,&st) && (uint64_t)st.st_size==archive_sizes[archive]))
            {shared_roots[archive]=root;atomic_fetch_add(&shared_root_count,1);}
        else if(root>=0){close(root);errno=EINVAL;}
    }
    int fd=shared_roots[archive]<0?-1:dup(shared_roots[archive]);
    int error=errno;
    pthread_mutex_unlock(&root_lock);
    if(fd<0){errno=error;return -1;}
    gangstar_archive_track(fd,path,O_RDONLY);
    struct Handle *h=acquire(fd);
    if(!h){close(fd);errno=EMFILE;return -1;}
    h->shared=1;h->known_offset=0;h->offset_valid=1;
    unsigned live=atomic_fetch_add(&shared_readers,1)+1,peak=atomic_load(&shared_peak);
    while(live>peak && !atomic_compare_exchange_weak(&shared_peak,&peak,live)){}
    release(h);
    return fd;
}

static ssize_t backing_read(void *context, void *buffer, size_t size, uint64_t offset) {
    struct BackingContext *io = context;
    if (offset > INT32_MAX) { errno = EOVERFLOW; return -1; }
    uint64_t started = (io->tracing || io->audio) ? trace_clock() : 0;
    ssize_t result;
    // Newlib pread may emulate seek/read/restore. Serialize physical reads on
    // duplicates of the same root; logical cursor changes never touch it.
    pthread_mutex_lock(&physical_lock[io->archive]);
#if defined(__vita__) && defined(GANGSTAR_NATIVE_ARCHIVE_IO)
    int native = native_fds[io->archive];
    if (native >= 0) {
        result = sceIoPread(native, buffer, size, offset);
        atomic_fetch_add(&native_calls, 1);
        if (result > 0) atomic_fetch_add(&native_bytes, (unsigned)result);
        else {
            // Preserve POSIX errors and a usable fallback without treating a
            // native SceUID as a newlib descriptor (or vice versa).
            atomic_fetch_add(&native_fallbacks, 1);
            result = pread(io->fd, buffer, size, (off_t)offset);
        }
    } else result = pread(io->fd, buffer, size, (off_t)offset);
#else
    result = pread(io->fd, buffer, size, (off_t)offset);
#endif
    int saved_errno = errno;
    pthread_mutex_unlock(&physical_lock[io->archive]);
    if (io->tracing || io->audio) {
        uint64_t elapsed=trace_clock()-started;
        if(io->audio)trace_audio_phase(RIO_AUDIO_DISK,elapsed);
        io->disk_us += elapsed;
        if (!io->disk_calls++) io->first_fill = offset;
        io->last_fill = offset;
        if (result > 0) io->disk_bytes += result;
    }
    errno = saved_errno;
    return result;
}

#ifdef __vita__
static struct { int archive; uint64_t offset, end; } prefetch_jobs[4];
static unsigned prefetch_count;
static pthread_mutex_t prefetch_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t prefetch_changed = PTHREAD_COND_INITIALIZER;
static atomic_int prefetch_ready;
static atomic_uint pf_queued, pf_done, pf_skipped, pf_errors, pf_bytes, pf_us;
static int cache_contains(int archive, uint64_t offset) {
    int found = 0;
    pthread_mutex_lock(&cache.lock);
    for (unsigned i = 0; i < cache.block_count; ++i) {
        RioArchiveBlock *b = &cache.blocks[i];
        if ((b->valid || b->loading) && b->archive == archive && b->offset == offset) {
            found = 1; break;
        }
    }
    pthread_mutex_unlock(&cache.lock);
    return found;
}
static void *prefetch_worker(void *unused) {
    (void)unused;
    sceKernelChangeThreadPriority(0, 191);
    const char *paths[] = {
        "ux0:/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb",
        "ux0:/data/gangstarrio/patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb"
    };
    int fds[] = {-1, -1}; // Private descriptors live until process exit.
    for (;;) {
        pthread_mutex_lock(&prefetch_lock);
        while (!prefetch_count) pthread_cond_wait(&prefetch_changed, &prefetch_lock);
        pthread_mutex_unlock(&prefetch_lock);
        // Do not start speculative I/O while a foreground archive call is active.
        // An already submitted read is allowed to finish; at most one block.
        if (atomic_load(&active_demand)) { sceKernelDelayThread(1000); continue; }
        pthread_mutex_lock(&prefetch_lock);
        int archive = prefetch_jobs[0].archive;
        uint64_t offset = prefetch_jobs[0].offset;
        if (!rio_prefetch_advance(&prefetch_jobs[0].offset, prefetch_jobs[0].end, RIO_CACHE_BLOCK_SIZE)) {
            --prefetch_count;
            memmove(prefetch_jobs, prefetch_jobs + 1, prefetch_count * sizeof(*prefetch_jobs));
        }
        pthread_mutex_unlock(&prefetch_lock);
        if (cache_contains(archive, offset)) { atomic_fetch_add(&pf_skipped, 1); continue; }
        if (fds[archive] == -1) {
            fds[archive] = open(paths[archive], O_RDONLY);
            struct stat st;
            if (fds[archive] < 0 || fstat(fds[archive], &st) ||
                (uint64_t)st.st_size != archive_sizes[archive]) {
                if (fds[archive] >= 0) close(fds[archive]);
                fds[archive] = -2;
            }
        }
        if (fds[archive] < 0) { atomic_fetch_add(&pf_errors, 1); continue; }
        struct BackingContext context = {.fd = fds[archive], .tracing = 1, .archive = archive};
        unsigned char byte;
        ssize_t result = rio_archive_cache_read(&cache, archive, archive_sizes[archive],
            &byte, 1, offset, backing_read, &context);
        atomic_fetch_add(result == 1 ? &pf_done : &pf_errors, 1);
        atomic_fetch_add(&pf_bytes, (unsigned)context.disk_bytes);
        atomic_fetch_add(&pf_us, (unsigned)context.disk_us);
    }
    return NULL;
}
static void prefetch_init(void) {
#if defined(GANGSTAR_DISABLE_PREFETCH) && !defined(GANGSTAR_ASSET_PREFETCH)
    l_info("Gangstar archive prefetch disabled");
    return;
#endif
    pthread_attr_t attr;
    if (pthread_attr_init(&attr)) return;
    pthread_t worker;
    int error = pthread_attr_setstacksize(&attr, 512 * 1024);
    if (!error) error = pthread_create(&worker, &attr, prefetch_worker, NULL);
    pthread_attr_destroy(&attr);
    if (error) { l_warn("Gangstar prefetch unavailable: %d", error); return; }
    pthread_detach(worker);
    atomic_store(&prefetch_ready, 1);
#ifdef GANGSTAR_ASSET_PREFETCH
    l_info("Gangstar asset prefetch: native texture ranges, four jobs, max 4 MiB each, one block per idle check");
#endif
#ifndef GANGSTAR_DISABLE_PREFETCH
    l_info("Gangstar prefetch: sequential 64 KiB trigger, one-block lookahead, four queued jobs");
#endif
}
static void prefetch_range(int archive, uint64_t offset, uint64_t end) {
    if (!atomic_load(&prefetch_ready)) return;
    pthread_mutex_lock(&prefetch_lock);
    for (unsigned i = 0; i < prefetch_count; ++i)
        if (prefetch_jobs[i].archive == archive && prefetch_jobs[i].offset <= offset && prefetch_jobs[i].end >= end) {
            pthread_mutex_unlock(&prefetch_lock); return;
        }
    if (prefetch_count == 4) {
        memmove(prefetch_jobs, prefetch_jobs + 1, 3 * sizeof(*prefetch_jobs));
        --prefetch_count;
        atomic_fetch_add(&pf_skipped, 1);
    }
    prefetch_jobs[prefetch_count].archive = archive;
    prefetch_jobs[prefetch_count].offset = offset;
    prefetch_jobs[prefetch_count++].end = end;
    atomic_fetch_add(&pf_queued, 1);
    pthread_cond_signal(&prefetch_changed);
    pthread_mutex_unlock(&prefetch_lock);
}
static void prefetch_enqueue(int archive, uint64_t offset) {
#ifndef GANGSTAR_DISABLE_PREFETCH
    prefetch_range(archive, offset, offset + RIO_CACHE_BLOCK_SIZE);
#else
    (void)archive; (void)offset;
#endif
}
void gangstar_archive_prefetch_asset(int archive, uint64_t offset, uint64_t size) {
#ifdef GANGSTAR_ASSET_PREFETCH
    uint64_t first, end;
    if (!atomic_load(&enabled) || archive < 0 || archive > 1 ||
        !rio_prefetch_asset_bounds(offset, size, archive_sizes[archive], RIO_CACHE_BLOCK_SIZE, &first, &end)) return;
    prefetch_range(archive, first, end);
#else
    (void)archive; (void)offset; (void)size;
#endif
}
#else
void gangstar_archive_prefetch_asset(int archive, uint64_t offset, uint64_t size) {
    (void)archive; (void)offset; (void)size;
}
static void prefetch_init(void) {}
static void prefetch_enqueue(int archive, uint64_t offset) { (void)archive; (void)offset; }
#endif

static void predict_next(struct Handle *h, off_t offset, ssize_t result) {
    uint64_t next;
    if (offset < 0 || result <= 0) { rio_prefetch_reset(&h->prediction); return; }
    if (rio_prefetch_next(&h->prediction, offset, (size_t)result,
        archive_sizes[h->archive], RIO_CACHE_BLOCK_SIZE, &next))
        prefetch_enqueue(h->archive, next);
}

ssize_t gangstar_archive_read(int fd, void *buffer, size_t size) {
    struct BackingContext context = {.fd = fd, .tracing = trace_main(), .audio = trace_audio()};
    uint64_t started = (context.tracing || context.audio) ? trace_clock() : 0;
    struct Handle *h = acquire(fd);
    if (!h) return read(fd, buffer, size);
    context.archive = h->archive;
    atomic_fetch_add(&active_demand, 1);
    uint64_t query_started=context.audio?trace_clock():0;
    off_t offset;
    if(h->shared)offset=h->known_offset;else
#if defined(GANGSTAR_ARCHIVE_READ_POSITION_CACHE) || defined(GANGSTAR_ARCHIVE_VIRTUAL_CURSOR)
    if(h->stream && h->offset_valid && !h->exposed) {
        offset=h->known_offset;
        atomic_fetch_add(&read_position_hits,1);
    } else
#endif
    {
        offset=lseek(fd,0,SEEK_CUR);
        atomic_fetch_add(&read_position_fallbacks,1);
    }
    if(context.audio)trace_audio_phase(RIO_AUDIO_POSITION_QUERY,trace_clock()-query_started);
    h->offset_valid=offset>=0;h->known_offset=offset;
    ssize_t result = -1;
    if (offset >= 0) {
        result = rio_archive_cache_read(&cache, h->archive, archive_sizes[h->archive],
            buffer, size, (uint64_t)offset, backing_read, &context);
        if(result>0) {
            uint64_t set_started=context.audio?trace_clock():0;
            off_t advanced;
            if(h->shared)advanced=offset+result;else
#ifdef GANGSTAR_ARCHIVE_VIRTUAL_CURSOR
            if(h->stream && !h->exposed) {
                advanced=offset+result; h->dirty=1;
                atomic_fetch_add(&virtual_advances,1);
            } else
#endif
                advanced=lseek(fd,offset+result,SEEK_SET);
            if(context.audio)trace_audio_phase(RIO_AUDIO_POSITION_SET,trace_clock()-set_started);
            h->offset_valid=advanced>=0;h->known_offset=advanced;
            if(advanced<0)result=-1;
        }
    }
    int archive = h->archive;
    predict_next(h, offset, result);
    atomic_fetch_sub(&active_demand, 1);
    release(h);
    int saved_errno = errno;
    if(context.audio)trace_audio_phase(RIO_AUDIO_CACHE,trace_clock()-started);
    trace_read(&context, archive, offset, size, result, started);
    errno = saved_errno;
    return result;
}

ssize_t gangstar_archive_pread(int fd, void *buffer, size_t size, off_t offset) {
    struct BackingContext context = {.fd = fd, .tracing = trace_main(), .audio = trace_audio()};
    uint64_t started = (context.tracing || context.audio) ? trace_clock() : 0;
    struct Handle *h = acquire(fd);
    if (!h) return pread(fd, buffer, size, offset);
    context.archive = h->archive;
    atomic_fetch_add(&active_demand, 1);
    ssize_t result;
    if (offset < 0) { errno = EINVAL; result = -1; }
    else result = rio_archive_cache_read(&cache, h->archive, archive_sizes[h->archive],
        buffer, size, (uint64_t)offset, backing_read, &context);
    int archive = h->archive;
    predict_next(h, offset, result);
    atomic_fetch_sub(&active_demand, 1);
    release(h);
    int saved_errno = errno;
    if(context.audio)trace_audio_phase(RIO_AUDIO_CACHE,trace_clock()-started);
    trace_read(&context, archive, offset, size, result, started);
    errno = saved_errno;
    return result;
}

off_t gangstar_archive_seek(int fd, off_t offset, int whence) {
    struct Handle *h = acquire(fd);
    if(h && h->shared) {
        int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?h->known_offset:
            whence==SEEK_END?(int64_t)archive_sizes[h->archive]:-1;
        int64_t target=base+(int64_t)offset;
        if(base<0 || target<0 || target>INT32_MAX) {release(h);errno=EINVAL;return -1;}
        h->known_offset=(off_t)target;h->offset_valid=1;
        release(h);return (off_t)target;
    }
    /* Deferred read advances must never be replaced by a stale kernel query,
       including when the independent tell-query shortcut is disabled. */
    if(h && h->dirty && whence==SEEK_CUR) {
        if(offset==0) { off_t result=h->known_offset;release(h);return result; }
        if(offset<INT32_MIN || offset>INT32_MAX) { release(h);errno=EINVAL;return -1; }
        int64_t target=(int64_t)h->known_offset+offset;
        if(target<0 || target>INT32_MAX) { release(h);errno=EINVAL;return -1; }
        offset=(off_t)target;whence=SEEK_SET;
    }
#ifdef GANGSTAR_ARCHIVE_POSITION_CACHE
    if(h && h->stream && offset==0 && whence==SEEK_CUR) {
        if(h->offset_valid && !h->exposed) {
            off_t result=h->known_offset;
            atomic_fetch_add(&position_hits,1);release(h);return result;
        }
        atomic_fetch_add(&position_fallbacks,1);
    }
#endif
    off_t result = lseek(fd, offset, whence);
    if(h) {
        if(result>=0) { h->offset_valid=1;h->known_offset=result;h->dirty=0; }
        else if(!h->dirty)h->offset_valid=0;
        release(h);
    }
    return result;
}
int gangstar_archive_expose(int fd) {
    struct Handle *h=acquire(fd);
    if(h) {
        if(h->shared) {h->exposed=1;release(h);return 0;}
        if(h->dirty) {
            if(lseek(fd,h->known_offset,SEEK_SET)<0) {
                int error=errno;atomic_fetch_add(&cursor_sync_errors,1);
                release(h);errno=error;return -1;
            }
            h->dirty=0;atomic_fetch_add(&cursor_syncs,1);
        }
        if(!h->exposed)atomic_fetch_add(&position_exposures,1);
        h->exposed=1;release(h);
    }
    return 0;
}

static int close_handle(int fd, int close_fd) {
    if (!enabled || fd < 0) return close_fd ? close(fd) : 0;
    pthread_mutex_lock(&table_lock);
    for (unsigned i = 0; i < MAX_HANDLES; ++i) if (handles[i].fd == fd) {
        struct Handle *h = &handles[i];
        h->closing = 1;
        while (h->references) pthread_cond_wait(&table_changed, &table_lock);
        int result = close_fd ? close(fd) : 0;
        if (!result) { if(h->shared)atomic_fetch_sub(&shared_readers,1);h->shared=0;h->fd = -1;h->stream = NULL; }
        h->closing = 0;
        pthread_mutex_unlock(&table_lock);
        return result;
    }
    pthread_mutex_unlock(&table_lock);
    return close_fd ? close(fd) : 0;
}

int gangstar_archive_close(int fd) { return close_handle(fd, 1); }
void gangstar_archive_forget(int fd) { (void)close_handle(fd, 0); }

struct ArchiveStream { FILE *original; int fd; };
static ssize_t stream_read(void *context, char *buffer, size_t count) {
    return gangstar_archive_read(((struct ArchiveStream *)context)->fd, buffer, count);
}
#ifdef __LARGE64_FILES
typedef _off64_t ArchiveOffset;
#else
typedef off_t ArchiveOffset;
#endif
static int stream_seek(void *context, ArchiveOffset *offset, int whence) {
    if (*offset < INT32_MIN || *offset > INT32_MAX) { errno = EOVERFLOW; return -1; }
    off_t result = gangstar_archive_seek(((struct ArchiveStream *)context)->fd, (off_t)*offset, whence);
    if (result < 0) return -1;
    *offset = result;
    return 0;
}
static int stream_close(void *context) {
    struct ArchiveStream *cookie = context;
    gangstar_archive_forget(cookie->fd);
    int result = fclose(cookie->original);
    free(cookie);
    return result;
}
FILE *gangstar_archive_stream(FILE *original, const char *path, const char *mode) {
    if (!original || !enabled || mode[0] != 'r' || strchr(mode, '+')) return original;
    int fd = fileno(original);
    if (path) gangstar_archive_track(fd, path, O_RDONLY);
    struct Handle *handle = acquire(fd);
    if (!handle) return original;
    if (handle->stream) { release(handle); return original; }
    int shared=handle->shared;
    release(handle);
    struct ArchiveStream *cookie = malloc(sizeof(*cookie));
    if (!cookie) { gangstar_archive_forget(fd); if(shared){int error=errno;fclose(original);errno=error;return NULL;}return original; }
    *cookie = (struct ArchiveStream){original, fd};
    cookie_io_functions_t functions = {.read = stream_read, .seek = stream_seek, .close = stream_close};
    FILE *stream = fopencookie(cookie, mode, functions);
    if (!stream) { int error=errno;free(cookie);gangstar_archive_forget(fd);if(shared){fclose(original);errno=error;return NULL;}return original; }
    pthread_mutex_lock(&table_lock);
    for (unsigned i = 0; i < MAX_HANDLES; ++i)
        if (handles[i].fd == fd) { handles[i].stream = stream; break; }
    pthread_mutex_unlock(&table_lock);
    return stream;
}
FILE *gangstar_archive_fdopen(int fd, const char *mode) {
    if(gangstar_archive_expose(fd)<0)return NULL;
    return gangstar_archive_stream(fdopen(fd, mode), NULL, mode);
}
int gangstar_archive_fileno(FILE *stream) {
    int fd = -1;
    if (enabled) {
        pthread_mutex_lock(&table_lock);
        for (unsigned i = 0; i < MAX_HANDLES; ++i)
            if (handles[i].fd >= 0 && handles[i].stream == stream) { fd = handles[i].fd; break; }
        pthread_mutex_unlock(&table_lock);
    }
    if(fd>=0) { if(gangstar_archive_expose(fd)<0)return -1;return fd; }
    return fileno(stream);
}

void gangstar_archive_report(void) {
    if (!enabled) return;
    l_info("RIO_ARCHIVE_SHARED physical_roots=%u readers=%u peak_readers=%u",
        atomic_load(&shared_root_count),atomic_load(&shared_readers),atomic_load(&shared_peak));
#ifdef __vita__
    SceKernelFreeMemorySizeInfo headroom={.size=sizeof(headroom)};
    int memory_result=sceKernelGetFreeMemorySize(&headroom);
    struct mallinfo heap=mallinfo();
    l_info("RIO_CACHE_HEADROOM actual_mib=%u user_free=%d heap_used=%u heap_arena=%u",
        (unsigned)(cache.block_count*RIO_CACHE_BLOCK_SIZE/(1024*1024)),
        memory_result<0?-1:headroom.size_user,(unsigned)heap.uordblks,(unsigned)heap.arena);
#endif

    (void)position_hits;(void)position_fallbacks;
    l_info("RIO_ARCHIVE_CURSOR deferred=%u synchronized=%u sync_errors=%u",
        atomic_exchange(&virtual_advances,0),atomic_exchange(&cursor_syncs,0),atomic_exchange(&cursor_sync_errors,0));
    l_info("RIO_ARCHIVE_READ_POSITION cached=%u queried=%u",
        atomic_exchange(&read_position_hits,0),atomic_exchange(&read_position_fallbacks,0));
    l_info("RIO_ARCHIVE_POSITION cached_queries=%u fallback_queries=%u exposed_handles=%u",
        atomic_exchange(&position_hits,0),atomic_exchange(&position_fallbacks,0),atomic_exchange(&position_exposures,0));
    RioArchiveStats s = rio_archive_cache_stats(&cache);
    (void)s;
    l_info("RIO_CACHE hit_bytes=%llu miss_bytes=%llu disk_bytes=%llu fills=%llu",
        (unsigned long long)s.hit_bytes, (unsigned long long)s.miss_bytes,
        (unsigned long long)s.disk_bytes, (unsigned long long)s.fills);
    for (unsigned i = 0; i < trace_count; ++i) {
        struct ReadTrace *t = &traces[i];
        (void)t;
        l_info("RIO_ARCHIVE frame=%u archive=%d offset=%llu requested=%llu returned=%llu elapsed_us=%llu disk_bytes=%llu disk_us=%llu disk_calls=%u first_fill=%llu last_fill=%llu",
            t->frame, t->archive, (unsigned long long)t->offset,
            (unsigned long long)t->requested, (unsigned long long)t->returned,
            (unsigned long long)t->elapsed, (unsigned long long)t->disk_bytes,
            (unsigned long long)t->disk_us, t->disk_calls,
            (unsigned long long)t->first_fill, (unsigned long long)t->last_fill);
    }
    if (trace_dropped) { l_info("RIO_ARCHIVE dropped=%u", trace_dropped); }
    trace_count = trace_dropped = 0;
#if defined(__vita__) && defined(GANGSTAR_NATIVE_ARCHIVE_IO)
    l_info("RIO_NATIVE_IO calls=%u bytes=%u fallbacks=%u",
        atomic_exchange(&native_calls, 0), atomic_exchange(&native_bytes, 0),
        atomic_exchange(&native_fallbacks, 0));
#endif
#ifdef __vita__
    l_info("RIO_PREFETCH queued=%u completed=%u skipped=%u errors=%u disk_bytes=%u disk_us=%u",
        atomic_exchange(&pf_queued, 0), atomic_exchange(&pf_done, 0),
        atomic_exchange(&pf_skipped, 0), atomic_exchange(&pf_errors, 0),
        atomic_exchange(&pf_bytes, 0), atomic_exchange(&pf_us, 0));
#endif
}
