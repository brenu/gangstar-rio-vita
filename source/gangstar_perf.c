#include <stdint.h>
#include "utils/logger.h"

void gangstar_perf_vitagl_report(uint32_t elapsed_us, uint32_t fixed_draw_us,
                              uint32_t fixed_draws, uint32_t fixed_state_us,
                              uint32_t shader_draw_us, uint32_t shader_draws,
                              uint32_t gpu_queue_us) {
    l_info("VGL_PROFILE elapsed_us=%u fixed_draw_us=%u fixed_draws=%u fixed_state_us=%u shader_draw_us=%u shader_draws=%u gpu_queue_us=%u",
           elapsed_us, fixed_draw_us, fixed_draws, fixed_state_us,
           shader_draw_us, shader_draws, gpu_queue_us);
}

