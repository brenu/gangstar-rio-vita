#ifndef GANGSTAR_NATIVE_PROFILE_H
#define GANGSTAR_NATIVE_PROFILE_H
#include <stdint.h>
#ifdef GANGSTAR_NATIVE_PROFILER
void gangstar_native_profile_install(void);
void gangstar_native_profile_frame(unsigned frame_number, uint64_t frame_us);
void gangstar_fs_trace_path(const char *path);
#else
static inline void gangstar_native_profile_install(void) {}
static inline void gangstar_native_profile_frame(unsigned frame_number, uint64_t frame_us) { (void)frame_number; (void)frame_us; }
static inline void gangstar_fs_trace_path(const char *path) { (void)path; }
#endif
#endif
