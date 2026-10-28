#ifndef GANGSTAR_SHADER_WARMUP_H
#define GANGSTAR_SHADER_WARMUP_H
#include <vitaGL.h>
void *gangstar_shader_capture(GLuint program);
void gangstar_shader_capture_finish(void *recipe, int linked);
void gangstar_shader_warmup(void);
#endif
