#ifndef GANGSTAR_PERF_POLICY_H
#define GANGSTAR_PERF_POLICY_H
#include <stdint.h>
typedef struct {
    uint64_t start, last_end, frames, work, swap, worst;
} GangstarPerfWindow;
static inline int gangstar_perf_add(GangstarPerfWindow *window, uint64_t begin,
                                 uint64_t end, GangstarPerfWindow *report) {
    if (end < begin || (window->last_end && begin < window->last_end)) {
        *window = (GangstarPerfWindow){0};
        return 0;
    }
    if (!window->last_end) {
        *window = (GangstarPerfWindow){.start=end, .last_end=end};
        return 0;
    }
    uint64_t frame = end - window->last_end;
    window->work += begin - window->last_end;
    window->swap += end - begin;
    if (frame > window->worst) window->worst = frame;
    ++window->frames;
    window->last_end = end;
    if (end - window->start < 5000000) return 0;
    *report = *window;
    *window = (GangstarPerfWindow){.start=end, .last_end=end};
    return 1;
}
static inline unsigned gangstar_perf_fps10(const GangstarPerfWindow *window) {
    uint64_t elapsed = window->last_end - window->start;
    return elapsed && window->frames ?
        (unsigned)((window->frames * 10000000 + elapsed/2) / elapsed) : 0;
}
#endif
