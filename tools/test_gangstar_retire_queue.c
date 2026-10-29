#include "retire_queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned released;
static void release(void *p) { ++released; free(p); }
int main(void) {
    void *queue[8] = {0};
    /* A failed allocation before and between valid transient buffers must
     * neither hide nor leak the rest of the frame's allocations. */
    for (unsigned frame = 0; frame < 20000; ++frame) {
        queue[1] = malloc(1024); queue[3] = malloc(2048); queue[7] = malloc(4096);
        assert(queue[1] && queue[3] && queue[7]);
        vgl_retire_queue_collect(queue, 8, release);
        assert(released == (frame + 1) * 3);
        for (unsigned i = 0; i < 8; ++i) assert(!queue[i]);
        vgl_retire_queue_collect(queue, 8, release);
        assert(released == (frame + 1) * 3);
    }
    puts("GPU retirement: holes, final slot, repeated frames and no double release passed");
}
