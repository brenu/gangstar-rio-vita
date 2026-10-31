#ifndef GANGSTAR_VERTEX_SCRATCH_H
#define GANGSTAR_VERTEX_SCRATCH_H
#include <stdint.h>
void gangstar_vertex_scratch_install(void);
int gangstar_vertex_extra_range(uintptr_t pointer,uintptr_t *end);
void gangstar_vertex_scratch_report(void);
void gangstar_vertex_scratch_frame_end(void);
#endif
