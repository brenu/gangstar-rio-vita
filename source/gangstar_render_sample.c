#include "gangstar_render_sample.h"
#ifdef GANGSTAR_SELECTIVE_VERTEX_COPY
#include "gangstar_vertex_scratch.h"
#endif
#include "utils/logger.h"
#include "gangstar_wait_perf.h"
#ifdef GANGSTAR_DETAILED_RENDER_PROFILE
#include <psp2/kernel/processmgr.h>
static struct {uint64_t us,work,worst;unsigned calls,samples;} sample[RIO_RENDER_KINDS];
extern void vglGetVertexPatchStats(unsigned *,unsigned *) __attribute__((weak));
extern void vglGetVertexRangeStats(unsigned *,unsigned *,uint64_t *) __attribute__((weak));
extern void vglGetSelectiveVertexStats(unsigned *,unsigned *,uint64_t *) __attribute__((weak));
extern void vglGetRejectedDrawSetupStats(unsigned *) __attribute__((weak));
extern void vglGetIndexScanStats(uint64_t [2],uint64_t [2]) __attribute__((weak));
static int elements_sampled;
static struct {uint64_t us,worst;unsigned calls;} stages[11];
static uint32_t random_state=0x61c88647u;
static struct {unsigned calls,samples;uint64_t us,worst;} vertex_copy;
uint64_t gangstar_vertex_copy_begin(void) {
 ++vertex_copy.calls;
 return (vertex_copy.calls&31u)==0?sceKernelGetProcessTimeWide():0;
}
void gangstar_vertex_copy_end(uint64_t began) {
 uint64_t us=sceKernelGetProcessTimeWide()-began;
 ++vertex_copy.samples;vertex_copy.us+=us;
 if(us>vertex_copy.worst)vertex_copy.worst=us;
}

uint64_t gangstar_render_begin(unsigned kind,unsigned work) {
#ifdef GANGSTAR_NATIVE_STARTUP_PROFILE
 // GL calls use the main context; sample clocks on roughly 1/32 calls.
 if(kind==RIO_RENDER_ELEMENTS)elements_sampled=0;
 if(kind>=RIO_RENDER_KINDS || !gangstar_wait_is_main())return 0;
 ++sample[kind].calls;sample[kind].work+=work;
 if(rio_render_choose(&random_state)) {if(kind==RIO_RENDER_ELEMENTS)elements_sampled=1;return sceKernelGetProcessTimeWide();}
#else
 (void)kind;(void)work;
#endif
 return 0;
}
void gangstar_render_end(unsigned kind,uint64_t began) {
 if(kind==RIO_RENDER_ELEMENTS)elements_sampled=0;
 if(!began)return;
 uint64_t elapsed=sceKernelGetProcessTimeWide()-began;
 sample[kind].us+=elapsed;++sample[kind].samples;
 if(elapsed>sample[kind].worst)sample[kind].worst=elapsed;
}
uint64_t gangstar_render_stage_clock(void) {return elements_sampled?sceKernelGetProcessTimeWide():0;}
void gangstar_render_stage_end(unsigned stage,uint64_t began) {
 if(!began || stage>=11)return;
 uint64_t elapsed=sceKernelGetProcessTimeWide()-began;
 stages[stage].us+=elapsed;++stages[stage].calls;
 if(elapsed>stages[stage].worst)stages[stage].worst=elapsed;
}
void gangstar_render_report(void) {
 if(vglGetIndexScanStats) {
  uint64_t calls[2],elements[2];vglGetIndexScanStats(calls,elements);
  l_info("RIO_INDEX_SCAN implementation=typed_four_lane u16_calls=%llu u16_indices=%llu u32_calls=%llu u32_indices=%llu",
   (unsigned long long)calls[0],(unsigned long long)elements[0],(unsigned long long)calls[1],(unsigned long long)elements[1]);
 }

 l_info("RIO_VERTEX_COPY_TIME calls=%u samples=%u sampled_us=%llu worst_sample_us=%llu",
  vertex_copy.calls,vertex_copy.samples,(unsigned long long)vertex_copy.us,(unsigned long long)vertex_copy.worst);
 vertex_copy=(typeof(vertex_copy)){0};
 if(vglGetRejectedDrawSetupStats) {
  unsigned rejected;vglGetRejectedDrawSetupStats(&rejected);
  l_info("RIO_DRAW_SETUP rejected=%u",rejected);
 }
#ifdef GANGSTAR_SELECTIVE_VERTEX_COPY
 gangstar_vertex_scratch_report();
#endif
 if(vglGetSelectiveVertexStats) {
  unsigned copied,direct;uint64_t bytes;vglGetSelectiveVertexStats(&copied,&direct,&bytes);
  l_info("RIO_SELECTIVE_VERTEX copied_draws=%u direct_draws=%u copied_bytes=%llu",copied,direct,(unsigned long long)bytes);
 }
 if(vglGetVertexRangeStats) {
  unsigned draws,trimmed;uint64_t saved;vglGetVertexRangeStats(&draws,&trimmed,&saved);
  l_info("RIO_VERTEX_RANGE draws=%u trimmed=%u saved_prefix_vertices=%llu",draws,trimmed,(unsigned long long)saved);
 }
 if(vglGetVertexPatchStats) {
  unsigned hits,misses;vglGetVertexPatchStats(&hits,&misses);
  l_info("RIO_VERTEX_PATCH hits=%u misses=%u",hits,misses);
 }
 static const char *stage_names[]={"scene_reset","vertex_shader_setup","index_prepare","gxm_submit","fragment_textures","vertex_prepare","vertex_patch","uniform_streams","vertex_layout","vertex_index_scan","vertex_copy"};
 for(unsigned i=0;i<11;++i) {
  l_info("RIO_DRAW_STAGE op=%s samples=%u sampled_us=%llu worst_sample_us=%llu",stage_names[i],stages[i].calls,(unsigned long long)stages[i].us,(unsigned long long)stages[i].worst);
  stages[i]=(typeof(stages[i])){0};
 }
 static const char *names[]={"draw_arrays","draw_elements","matrix4","uniform4","uniform1","uniform_location","draw_sync"};
 for(unsigned i=0;i<RIO_RENDER_KINDS;++i) {
  l_info("RIO_RENDER_SAMPLE op=%s calls=%u work=%llu samples=%u sampled_us=%llu worst_sample_us=%llu",
   names[i],sample[i].calls,(unsigned long long)sample[i].work,sample[i].samples,
   (unsigned long long)sample[i].us,(unsigned long long)sample[i].worst);
  sample[i]=(typeof(sample[i])){0};
 }
}

#else
// ABI stubs keep existing VitaGL weak callbacks valid without clock reads,
// thread checks, counters, or per-draw diagnostic collection.
uint64_t gangstar_vertex_copy_begin(void){return 0;}
void gangstar_vertex_copy_end(uint64_t began){(void)began;}
uint64_t gangstar_render_begin(unsigned kind,unsigned work){(void)kind;(void)work;return 0;}
void gangstar_render_end(unsigned kind,uint64_t began){(void)kind;(void)began;}
uint64_t gangstar_render_stage_clock(void){return 0;}
void gangstar_render_stage_end(unsigned stage,uint64_t began){(void)stage;(void)began;}
extern void vglGetRejectedDrawSetupStats(unsigned *) __attribute__((weak));
void gangstar_render_report(void) {
 if(vglGetRejectedDrawSetupStats) {
  unsigned rejected;vglGetRejectedDrawSetupStats(&rejected);
  if(rejected)l_warn("RIO_DRAW_SETUP rejected=%u",rejected);
 }
}
#endif
