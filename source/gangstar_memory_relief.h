#ifndef GANGSTAR_MEMORY_RELIEF_H
#define GANGSTAR_MEMORY_RELIEF_H
#include <stddef.h>
/* Optional RAM caches (archive sources, reload atlases) share the newlib heap
 * with the game and with textures that spill out of the GPU pools. Each call
 * drops their oldest entries and returns the bytes freed, 0 when nothing is
 * left to drop. Callers retry their allocation until it fits or this is 0. */
size_t gangstar_memory_relieve(size_t needed);
size_t gangstar_texture_source_relieve(size_t bytes);
size_t gangstar_texture_hot_relieve(size_t bytes);   // frame thread only
void gangstar_install_new_relief(void);
unsigned gangstar_memory_relief_count(void);
#endif
