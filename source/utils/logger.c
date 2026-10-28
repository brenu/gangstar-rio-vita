/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/logger.h"

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>

#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <psp2/kernel/processmgr.h>
#include "log_queue.h"
#include <string.h>
#include <psp2/io/fcntl.h>

static SceUID gangstar_log_fd = -1;

#define COLOR_RED    "\x1B[38;5;196m"
#define COLOR_PINK   "\x1B[38;5;212m"
#define COLOR_ORANGE "\x1B[38;5;202m"
#define COLOR_BLUE   "\x1B[38;5;32m"
#define COLOR_GREEN  "\x1B[32m"
#define COLOR_CYAN   "\x1B[36m"

#define COLOR_END    "\033[0m"

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lifecycle_lock = PTHREAD_MUTEX_INITIALIZER;
#ifdef GANGSTAR_ASYNC_LOG
static pthread_cond_t log_changed = PTHREAD_COND_INITIALIZER;
static pthread_t log_worker;
static RioLogQueue log_queue;
static int log_async, log_closing, log_busy;
static uint64_t log_written, log_errors, log_write_us, log_write_worst;
static void log_output(const char *bytes,size_t length) {
    sceClibPrintf("%.*s", (int)length, bytes);
    size_t offset=0;
    uint64_t began=sceKernelGetProcessTimeWide();
    while (gangstar_log_fd>=0 && offset<length) {
        int written=sceIoWrite(gangstar_log_fd,bytes+offset,length-offset);
        if (written<=0) break;
        offset+=(size_t)written;
    }
    uint64_t elapsed=sceKernelGetProcessTimeWide()-began;
    pthread_mutex_lock(&log_lock);
    log_written+=offset; if (offset!=length && gangstar_log_fd>=0) ++log_errors;
    log_write_us+=elapsed; if (elapsed>log_write_worst) log_write_worst=elapsed;
    pthread_mutex_unlock(&log_lock);
}
static void *log_dispatch(void *unused) {
    (void)unused;
    sceKernelChangeThreadPriority(0,191);
    char chunk[RIO_LOG_CHUNK];
    for (;;) {
        pthread_mutex_lock(&log_lock);
        while (!log_queue.used && !log_closing) pthread_cond_wait(&log_changed,&log_lock);
        size_t n=rio_log_pop(&log_queue,chunk,sizeof(chunk));
        if (!n && log_closing) { pthread_mutex_unlock(&log_lock); break; }
        log_busy=1; pthread_mutex_unlock(&log_lock);
        log_output(chunk,n);
        pthread_mutex_lock(&log_lock); log_busy=0;
        pthread_cond_broadcast(&log_changed); pthread_mutex_unlock(&log_lock);
    }
    return NULL;
}
#endif
void gangstar_log_start(void) {
    gangstar_log_finish();
    pthread_mutex_lock(&lifecycle_lock);
    pthread_mutex_lock(&log_lock);
    gangstar_log_fd=sceIoOpen(DATA_PATH "port.log",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
#ifdef GANGSTAR_ASYNC_LOG
    memset(&log_queue,0,sizeof(log_queue)); log_closing=log_busy=0;
    log_written=log_errors=log_write_us=log_write_worst=0;
    pthread_attr_t attr;
    int error=pthread_attr_init(&attr);
    if (!error) {
        error=pthread_attr_setstacksize(&attr,64*1024);
        if (!error) error=pthread_create(&log_worker,&attr,log_dispatch,NULL);
        pthread_attr_destroy(&attr);
    }
    log_async=!error;
#endif
    pthread_mutex_unlock(&log_lock);
    pthread_mutex_unlock(&lifecycle_lock);
}
void gangstar_log_report(void) {
#ifdef GANGSTAR_ASYNC_LOG
    pthread_mutex_lock(&log_lock);
    size_t pending=log_queue.used, peak=log_queue.peak;
    uint64_t dropped=log_queue.dropped, written=log_written, errors=log_errors;
    uint64_t elapsed=log_write_us,worst=log_write_worst;
    int active=log_async;
    log_write_us=log_write_worst=0;
    pthread_mutex_unlock(&log_lock);
    l_info("RIO_LOG async=%d pending=%u peak=%u dropped=%llu written=%llu errors=%llu worker_write_us=%llu worker_worst_us=%llu",
        active,(unsigned)pending,(unsigned)peak,(unsigned long long)dropped,
        (unsigned long long)written,(unsigned long long)errors,
        (unsigned long long)elapsed,(unsigned long long)worst);
#endif
}

// Buffer A is used to adjust the format string.
static char buffer_a[2048];
// Buffer B is used to compile the final log using the updated format string.
static char buffer_b[2048];

void _log_print(int t, const char* fmt, ...) {
    pthread_mutex_lock(&log_lock);

    switch (t) {
        case LT_DEBUG:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s• debug%s    %s\n",
                            COLOR_PINK, COLOR_END, fmt); break;
        case LT_INFO:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %sℹ info%s     %s\n",
                            COLOR_BLUE, COLOR_END, fmt); break;
        case LT_WARN:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⚠ warning%s  %s\n",
                            COLOR_ORANGE, COLOR_END, fmt); break;
        case LT_ERROR:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⨯ error%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_FATAL:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! fatal%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_SUCCESS:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! success%s  %s\n",
                            COLOR_GREEN, COLOR_END, fmt); break;
        case LT_WAIT:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s… waiting%s  %s\n",
                            COLOR_CYAN, COLOR_END, fmt); break;
        default:
            pthread_mutex_unlock(&log_lock);
            return;
    }

    va_list list;
    va_start(list, fmt);
    sceClibVsnprintf(buffer_b, sizeof(buffer_b), buffer_a, list);
    va_end(list);
#ifdef GANGSTAR_ASYNC_LOG
    if (log_async) {
        size_t length=strlen(buffer_b);
        char fatal_record[2048]; const char *record=buffer_b;
        if (t==LT_FATAL) {
            memcpy(fatal_record,buffer_b,length); record=fatal_record;
            while (!log_closing && length>RIO_LOG_CAPACITY-log_queue.used)
                pthread_cond_wait(&log_changed,&log_lock);
        }
        if (!log_closing && rio_log_push(&log_queue,record,length)) pthread_cond_signal(&log_changed);
        // Fatal evidence is drained before the caller can abort. Ordinary
        // diagnostic records never wait for storage, including a full queue.
        if (t==LT_FATAL && !log_closing)
            while (log_queue.used || log_busy) pthread_cond_wait(&log_changed,&log_lock);
        pthread_mutex_unlock(&log_lock);
        return;
    }
#endif
    sceClibPrintf("%s",buffer_b);
    size_t total=strlen(buffer_b),offset=0;
    while (gangstar_log_fd>=0 && offset<total) {
        int written=sceIoWrite(gangstar_log_fd,buffer_b+offset,total-offset);
        if (written<=0) break;
        offset+=(size_t)written;
    }
    pthread_mutex_unlock(&log_lock);
}

void gangstar_log_finish(void) {
    pthread_mutex_lock(&lifecycle_lock);
    pthread_mutex_lock(&log_lock);
#ifdef GANGSTAR_ASYNC_LOG
    int join=log_async;
    if (join) { log_closing=1; pthread_cond_signal(&log_changed); }
    pthread_mutex_unlock(&log_lock);
    if (join) pthread_join(log_worker,NULL);
    pthread_mutex_lock(&log_lock);
    log_async=0;
#endif
    SceUID fd=gangstar_log_fd; gangstar_log_fd=-1;
    pthread_mutex_unlock(&log_lock);
    if (fd>=0) { sceIoSyncByFd(fd,0); sceIoClose(fd); }
    pthread_mutex_unlock(&lifecycle_lock);
}
