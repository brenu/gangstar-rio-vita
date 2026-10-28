/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/io.h"
#include "gangstar_archive_cache.h"
#include "gangstar_obb_path.h"
#include "gangstar_loose_index.h"
#include "gangstar_file_names.h"
#include "gangstar_audio_probe.h"
#include "gangstar_wait_perf.h"
#include <limits.h>

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdatomic.h>
#include <dirent.h>
#include <stdarg.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#include "utils/logger.h"
#include "utils/utils.h"

// Includes the following inline utilities:
// int oflags_musl_to_newlib(int flags);
// dirent64_bionic * dirent_newlib_to_bionic(struct dirent* dirent_newlib);
// void stat_newlib_to_bionic(struct stat * src, stat64_bionic * dst);
#include "reimpl/bits/_struct_converters.c"

// Some GLUtils save paths append Windows separators even in the Android
// binary. Vita's filesystem needs forward slashes, including these save paths.
static const char *gangstar_io_path(const char *path, char out[PATH_MAX]) {
    if (!strchr(path, '\\')) return path;
    size_t n = 0;
    for (; *path; ++path) {
        char c = *path == '\\' ? '/' : *path;
        if (c == '/' && n && out[n - 1] == '/') continue;
        if (n + 1 >= PATH_MAX) { errno = ENAMETOOLONG; return NULL; }
        out[n++] = c;
    }
    out[n] = 0;
    return out;
}

static atomic_uint rio_read_calls, rio_read_bytes, rio_read_us, rio_read_worst;
static atomic_uint rio_worst_bytes, rio_worst_caller, rio_worst_tid;
static atomic_uint rio_rejected_probes, rio_rejected_obb_children, rio_rejected_embedded_obb;
static int reject_obb_child(const char *path) {
    int error=rio_obb_path_error(path);
    if(!error)return 0;
    atomic_fetch_add_explicit(&rio_rejected_obb_children,1,memory_order_relaxed);
    if(error==EINVAL)atomic_fetch_add_explicit(&rio_rejected_embedded_obb,1,memory_order_relaxed);
    errno=error;
    return 1;
}
static atomic_uint rio_buffered_reads,rio_buffered_writes,rio_buffer_failures;
static atomic_int rio_frame_thread;
// Only the registered frame thread accesses this sample. Background reads
// remain in the existing aggregate counters, not the frame's elapsed time.
static GangstarFrameIo rio_frame_io;
void gangstar_io_frame_begin(void) {
    atomic_store_explicit(&rio_frame_thread, sceKernelGetThreadId(), memory_order_relaxed);
    rio_frame_io = (GangstarFrameIo){0};
}
GangstarFrameIo gangstar_io_frame_end(void) { return rio_frame_io; }
static unsigned rio_read_sample(uint64_t started, size_t bytes, uintptr_t caller) {
    unsigned elapsed = (unsigned)(sceKernelGetProcessTimeWide() - started);
    if(rio_audio_scope_active())rio_audio_phase(RIO_AUDIO_READ,elapsed);
    if (atomic_load_explicit(&rio_frame_thread, memory_order_relaxed) == sceKernelGetThreadId()) {
        rio_frame_io.read_us += elapsed;
        rio_frame_io.bytes += bytes;
        ++rio_frame_io.calls;
    }
    atomic_fetch_add_explicit(&rio_read_calls, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&rio_read_bytes, (unsigned)bytes, memory_order_relaxed);
    atomic_fetch_add_explicit(&rio_read_us, elapsed, memory_order_relaxed);
    unsigned worst = atomic_load_explicit(&rio_read_worst, memory_order_relaxed);
    while (elapsed > worst) {
        if (atomic_compare_exchange_weak_explicit(&rio_read_worst, &worst, elapsed,
            memory_order_relaxed, memory_order_relaxed)) {
            atomic_store(&rio_worst_bytes, (unsigned)bytes);
            atomic_store(&rio_worst_caller, (unsigned)caller);
            atomic_store(&rio_worst_tid, sceKernelGetThreadId());
            break;
        }
    }
    return elapsed;
}
static void rio_read_name(unsigned elapsed,uint64_t started,unsigned kind,uintptr_t key,
    size_t requested,size_t returned,uintptr_t caller,const char *operation) {
    if(elapsed<1000 || !gangstar_wait_is_main())return;
    int saved=errno;char path[192];rio_file_name_get(kind,key,path);
    gangstar_wait_end_context(RIO_WAIT_FILE,started,caller,key,requested,
        returned>INT_MAX?INT_MAX:(int)returned,operation,path,0);
    errno=saved;
}
void gangstar_io_report(void) {
    RioLooseIndexStats loose=rio_loose_index_stats();
    l_info("RIO_LOOSE_INDEX absent=%u skipped=%u invalidations=%u",loose.absent,loose.hits,loose.invalidations);
    l_info("RIO_IO calls=%u bytes=%u read_us=%u worst_read_us=%u skipped_document_probes=%u skipped_obb_children=%u skipped_embedded_obb=%u",
        atomic_exchange(&rio_read_calls, 0), atomic_exchange(&rio_read_bytes, 0),
        atomic_exchange(&rio_read_us, 0), atomic_exchange(&rio_read_worst, 0),
        atomic_exchange(&rio_rejected_probes, 0),atomic_exchange(&rio_rejected_obb_children,0),atomic_exchange(&rio_rejected_embedded_obb,0));
#ifdef GANGSTAR_LOOSE_IO_BUFFERING
    l_info("RIO_FILE_BUFFER reads=%u writes=%u failures=%u read_buffer=16384 write_buffer=65536",
        atomic_exchange(&rio_buffered_reads,0),atomic_exchange(&rio_buffered_writes,0),atomic_exchange(&rio_buffer_failures,0));
#endif
    l_info("RIO_IO_WORST bytes=%u caller=0x%x tid=0x%x obb_buffer=16384",
        atomic_load(&rio_worst_bytes), atomic_load(&rio_worst_caller), atomic_load(&rio_worst_tid));
}

FILE * fopen_soloader(const char * filename, const char * mode) {
    char normalized[PATH_MAX];
    filename = gangstar_io_path(filename, normalized);
    if (!filename) return NULL;
    if(mode[0]!='r' || strchr(mode,'+'))rio_loose_index_mutation(filename);
    if (reject_obb_child(filename)) return NULL;
    if(rio_loose_index_missing(filename)) { errno=ENOENT;return NULL; }
    // This Android loose-file fallback contains a Vita device path inside a
    // relative Documents path. It can never name a Vita file; the resolver
    // subsequently opens the proper absolute OBB path. Avoid a storage probe
    // and synchronous warning for every streamed resource.
    if (!strncmp(filename, "../Documents/ux0:", strlen("../Documents/ux0:"))) {
        atomic_fetch_add_explicit(&rio_rejected_probes, 1, memory_order_relaxed);
        errno = ENOENT;
        return NULL;
    }
    if (strcmp(filename, "/proc/cpuinfo") == 0) {
        return fopen_soloader("app0:/cpuinfo", mode);
    } else if (strcmp(filename, "/proc/meminfo") == 0) {
        return fopen_soloader("app0:/meminfo", mode);
    }

#ifdef USE_SCELIBC_IO
    FILE* ret = sceLibcBridge_fopen(filename, mode);
#else
    FILE* ret = fopen(filename, mode);
    ret = gangstar_archive_stream(ret, filename, mode);
#ifdef GANGSTAR_LOOSE_IO_BUFFERING
    if(ret) {
        int writable=mode[0]=='w' || mode[0]=='a' || strchr(mode,'+');
        size_t buffer=writable?64*1024:16*1024;
        if(setvbuf(ret,NULL,_IOFBF,buffer))atomic_fetch_add(&rio_buffer_failures,1);
        else if(writable)atomic_fetch_add(&rio_buffered_writes,1);
        else atomic_fetch_add(&rio_buffered_reads,1);
    }
#else
    if (ret && mode[0] == 'r' && !strchr(mode, '+')) {
        size_t length = strlen(filename);
        if (length >= 4 && !strcmp(filename + length - 4, ".obb")) {
            // Newlib's small default buffer turns sequential archive reads
            // into many storage calls. FILE owns/frees this larger buffer;
            // standard fseek/ftell and EOF behavior stay in newlib.
            if (setvbuf(ret, NULL, _IOFBF, 16 * 1024))
                l_warn("Gangstar OBB buffering unavailable: %s", filename);
        }
    }
#endif
#endif

    if (ret)
        l_debug("fopen(%s, %s): %p", filename, mode, ret);
    else
#ifdef GANGSTAR_QUIET_DIAGNOSTICS
    // The native archive resolver routinely probes nonexistent loose files
    // before reading the OBB. Logging every ENOENT stalls the storage path.
    if (errno != ENOENT && errno != ENOTDIR)
#endif
        l_warn("fopen(%s, %s): %p", filename, mode, ret);
    static atomic_uint obb_opens;
    if (ret && strstr(filename, ".obb") && atomic_fetch_add(&obb_opens, 1) < 16)
        l_info("Gangstar OBB open: %s stream=%p", filename, ret);
#ifdef GANGSTAR_IO_PROFILE
    if (ret && strstr(filename, DATA_PATH))
        l_info("IO_FILE op=fopen path=%s stream=%p", filename, ret);
#endif

    if(ret)rio_file_name_set(1,(uintptr_t)ret,filename);
    return ret;
}

size_t fread_soloader(void *ptr, size_t size, size_t nmemb, FILE *stream) {
#ifdef USE_SCELIBC_IO
#ifdef GANGSTAR_IO_PROFILE
    if (size && nmemb <= SIZE_MAX / size && size * nmemb >= 4096) {
        uint64_t start = sceKernelGetProcessTimeWide();
        size_t result = sceLibcBridge_fread(ptr, size, nmemb, stream);
        uint64_t elapsed = sceKernelGetProcessTimeWide() - start;
        if (elapsed >= 2000)
            l_info("IO_SLOW op=fread stream=%p requested=%u returned=%u elapsed_us=%u",
                   stream, (unsigned)(size * nmemb), (unsigned)(result * size), (unsigned)elapsed);
        return result;
    }
#endif
    return sceLibcBridge_fread(ptr, size, nmemb, stream);
#else
#ifdef GANGSTAR_IO_PROFILE
    if (size && nmemb <= SIZE_MAX / size && size * nmemb >= 4096) {
        uint64_t start = sceKernelGetProcessTimeWide();
        size_t result = fread(ptr, size, nmemb, stream);
        uint64_t elapsed = sceKernelGetProcessTimeWide() - start;
        if (elapsed >= 2000)
            l_info("IO_SLOW op=fread stream=%p requested=%u returned=%u elapsed_us=%u",
                   stream, (unsigned)(size * nmemb), (unsigned)(result * size), (unsigned)elapsed);
        return result;
    }
#endif
    uint64_t started = sceKernelGetProcessTimeWide();
    size_t result = fread(ptr, size, nmemb, stream);
    size_t returned=size && result<=SIZE_MAX/size?result*size:0;
    unsigned elapsed=rio_read_sample(started,returned,(uintptr_t)__builtin_return_address(0));
    rio_read_name(elapsed,started,1,(uintptr_t)stream,size && nmemb<=SIZE_MAX/size?size*nmemb:0,
        returned,(uintptr_t)__builtin_return_address(0),"fread");
    return result;
#endif
}

ssize_t read_soloader(int fd, void *buf, size_t count) {
#ifdef GANGSTAR_IO_PROFILE
    if (count >= 4096) {
        off_t offset = lseek(fd, 0, SEEK_CUR);
        void *caller = __builtin_return_address(0);
        SceUID thread_id = sceKernelGetThreadId();
        uint64_t start = sceKernelGetProcessTimeWide();
        ssize_t result = gangstar_archive_read(fd, buf, count);
        uint64_t elapsed = sceKernelGetProcessTimeWide() - start;
        if (elapsed >= 2000)
            l_info("IO_SLOW op=read fd=%d offset=%lld requested=%u returned=%d elapsed_us=%u tid=%d caller=%p",
                   fd, (long long)offset, (unsigned)count, (int)result,
                   (unsigned)elapsed, (int)thread_id, caller);
        return result;
    }
#endif
    uint64_t started = sceKernelGetProcessTimeWide();
    ssize_t result = gangstar_archive_read(fd, buf, count);
    unsigned elapsed=rio_read_sample(started,result>0?(size_t)result:0,(uintptr_t)__builtin_return_address(0));
    rio_read_name(elapsed,started,2,(uintptr_t)fd,count,result>0?(size_t)result:0,
        (uintptr_t)__builtin_return_address(0),"read");
    return result;
}

ssize_t pread_soloader(int fd, void *buf, size_t count, off_t offset) {
    uint64_t started = sceKernelGetProcessTimeWide();
    ssize_t result = gangstar_archive_pread(fd, buf, count, offset);
    unsigned elapsed=rio_read_sample(started,result>0?(size_t)result:0,(uintptr_t)__builtin_return_address(0));
    rio_read_name(elapsed,started,2,(uintptr_t)fd,count,result>0?(size_t)result:0,
        (uintptr_t)__builtin_return_address(0),"pread");
    return result;
}

int open_soloader(const char * path, int oflag, ...) {
    char normalized[PATH_MAX];
    path = gangstar_io_path(path, normalized);
    if (!path) return -1;
    if((oflag & 3)!=0 || (oflag & (BIONIC_O_CREAT|BIONIC_O_TRUNC|BIONIC_O_TMPFILE)))
        rio_loose_index_mutation(path);
    if (reject_obb_child(path)) return -1;
    if(rio_loose_index_missing(path)) { errno=ENOENT;return -1; }
    if (strcmp(path, "/proc/cpuinfo") == 0) {
        return open_soloader("app0:/cpuinfo", oflag);
    } else if (strcmp(path, "/proc/meminfo") == 0) {
        return open_soloader("app0:/meminfo", oflag);
    } else if (strcmp(path, "/dev/urandom") == 0) {
        return open_soloader("app0:/urandom", oflag);
    }

    mode_t mode = 0666;
    if (((oflag & BIONIC_O_CREAT) == BIONIC_O_CREAT) ||
        ((oflag & BIONIC_O_TMPFILE) == BIONIC_O_TMPFILE)) {
        va_list args;
        va_start(args, oflag);
        mode = (mode_t)(va_arg(args, int));
        va_end(args);
    }

    oflag = oflags_bionic_to_newlib(oflag);
    int ret = open(path, oflag, mode);
    if (ret >= 0) {
        gangstar_archive_track(ret, path, oflag);
        gangstar_archive_expose(ret);
        rio_file_name_set(2,(uintptr_t)ret,path);
        l_debug("open(%s, %x): %i", path, oflag, ret);
    } else
        l_warn("open(%s, %x): %i", path, oflag, ret);
#ifdef GANGSTAR_IO_PROFILE
    if (ret >= 0 && strstr(path, DATA_PATH))
        l_info("IO_FILE op=open path=%s fd=%d", path, ret);
#endif
    return ret;
}

int fstat_soloader(int fd, stat64_bionic * buf) {
    struct stat st;
    int res = fstat(fd, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("fstat(%i): %i", fd, res);
    return res;
}

int stat_soloader(const char * path, stat64_bionic * buf) {
    char normalized[PATH_MAX];
    path = gangstar_io_path(path, normalized);
    if (!path) return -1;
    if (reject_obb_child(path)) return -1;
    if(rio_loose_index_missing(path)) { errno=ENOENT;return -1; }
    if (strcmp(path, "/system/lib/libOpenSLES.so") == 0) {
        l_debug("stat(%s): returning 0 in case this is a check for OpenSLES support", path);
        return 0;
    }

    struct stat st;
    int res = stat(path, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("stat(%s): %i size=%lld", path, res, res == 0 ? (long long)buf->st_size : -1LL);
    return res;
}

int fclose_soloader(FILE * f) {
    rio_file_name_clear(1,(uintptr_t)f);
#ifdef USE_SCELIBC_IO
    int ret = sceLibcBridge_fclose(f);
#else
    // fdopen may transfer ownership of a cached descriptor to a newlib FILE.
    gangstar_archive_forget(gangstar_archive_fileno(f));
    int ret = fclose(f);
#endif

    l_debug("fclose(%p): %i", f, ret);
    return ret;
}

int close_soloader(int fd) {
    rio_file_name_clear(2,(uintptr_t)fd);
    int ret = gangstar_archive_close(fd);
    l_debug("close(%i): %i", fd, ret);
    return ret;
}

DIR* opendir_soloader(char* _pathname) {
    DIR* ret = opendir(_pathname);
    l_debug("opendir(\"%s\"): %p", _pathname, ret);
    return ret;
}

struct dirent64_bionic * readdir_soloader(DIR * dir) {
    static struct dirent64_bionic dirent_tmp;

    struct dirent* ret = readdir(dir);
    l_debug("readdir(%p): %p", dir, ret);

    if (ret) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(ret);
        memcpy(&dirent_tmp, entry_tmp, sizeof(dirent64_bionic));
        free(entry_tmp);
        return &dirent_tmp;
    }

    return NULL;
}

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result) {
    struct dirent dirent_tmp;
    struct dirent * pdirent_tmp;

    int ret = readdir_r(dirp, &dirent_tmp, &pdirent_tmp);

    if (ret == 0) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(&dirent_tmp);
        memcpy(entry, entry_tmp, sizeof(dirent64_bionic));
        *result = (pdirent_tmp != NULL) ? entry : NULL;
        free(entry_tmp);
    }

    l_debug("readdir_r(%p, %p, %p): %i", dirp, entry, result, ret);
    return ret;
}

int closedir_soloader(DIR * dir) {
    int ret = closedir(dir);
    l_debug("closedir(%p): %i", dir, ret);
    return ret;
}

int fcntl_soloader(int fd, int cmd, ...) {
    l_warn("fcntl(%i, %i, ...): not implemented", fd, cmd);
    return 0;
}

int ioctl_soloader(int fd, int request, ...) {
    l_warn("ioctl(%i, %i, ...): not implemented", fd, request);
    return 0;
}

int fsync_soloader(int fd) {
    int ret = fsync(fd);
    l_debug("fsync(%i): %i", fd, ret);
    return ret;
}
