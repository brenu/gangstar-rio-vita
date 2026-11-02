#ifndef GANGSTAR_STREAM_PREFETCH_H
#define GANGSTAR_STREAM_PREFETCH_H
#include <stdint.h>
#ifdef GANGSTAR_STREAM_PREFETCH
int gangstar_stream_defer(const void *texture);
int gangstar_stream_defer_name(const char *name);
#else
static inline int gangstar_stream_defer(const void *texture){(void)texture;return 0;}
#endif
#endif
