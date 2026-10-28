/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/glutil.h"
#include "gangstar_asset_perf.h"
#include "gangstar_shader_warmup.h"

#include "utils/utils.h"
#include "utils/dialog.h"
#include "utils/logger.h"

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>

// Helpers for our handling of shaders
GLboolean skip_next_compile = GL_FALSE;
char next_shader_fname[256];
void load_shader(GLuint shader, const char * string, size_t length);

void gl_preload() {
    if (!file_exists("ur0:/data/libshacccg.suprx")
        && !file_exists("ur0:/data/external/libshacccg.suprx")) {
        fatal_error("Error: libshacccg.suprx is not installed. "
                    "Google \"ShaRKBR33D\" for quick installation.");
    }

#ifdef USE_GLSL_SHADERS
    vglSetSemanticBindingMode(VGL_MODE_POSTPONED);
#endif
}

static int graphics_ready;

void gl_init() {
    if (graphics_ready) return;
    SceKernelFreeMemorySizeInfo before = {.size = sizeof(before)};
    sceKernelGetFreeMemorySize(&before);
    l_info("Before vitaGL: free user=%u CDRAM=%u physical=%u bytes",
            before.size_user, before.size_cdram, before.size_phycont);
    // vglInitExtended's RAM argument is a free-memory threshold, not a pool
    // budget. Use bounded pools alongside the shared libc/game heap.
    // In this pinned vitaGL revision the return value reports resolution
    // fallback, NOT initialization success (GL_FALSE is normal at 960x544).
    GLboolean fallback = vglInitWithCustomSizes(0, 960, 544,
        16 * 1024 * 1024, 96 * 1024 * 1024, 8 * 1024 * 1024, 0, SCE_GXM_MULTISAMPLE_NONE);
    graphics_ready = 1;
    SceKernelFreeMemorySizeInfo after = {.size = sizeof(after)};
    sceKernelGetFreeMemorySize(&after);
    l_success("VitaGL init returned: resolution fallback=%d; pools RAM=16 CDRAM=96 physical=8 MiB",
              (int)fallback);
    l_info("After vitaGL: free user=%u CDRAM=%u physical=%u bytes",
            after.size_user, after.size_cdram, after.size_phycont);
}

void gl_swap() {
    vglSwapBuffers(GL_FALSE);
}

void gl_shutdown(void) {
    if (!graphics_ready) return;
    l_info("Graphics shutdown: waiting for GPU/display submissions");
    glFinish();
    int result = sceGxmDisplayQueueFinish();
    l_info("Graphics shutdown: display queue drained result=0x%x", result);
    // This pinned vitaGL has no public context-destruction API. The process
    // teardown releases its pools; do not free buffers still owned by GXM.
    // Keep graphics_ready true: the context still exists until process exit,
    // including if a later stack-guard diagnostic needs to display a message.
}

void glShaderSource_soloader(GLuint shader, GLsizei count,
                             const GLchar **string, const GLint *_length) {
#ifdef DEBUG_OPENGL
    sceClibPrintf("[gl_dbg] glShaderSource<%p>(shader: %i, count: %i, string: %p, length: %p)\n", __builtin_return_address(0), shader, count, string, _length);
#endif
    if (!string) {
        l_error("<%p> Shader source string is NULL, count: %i",
                   __builtin_return_address(0), count);
        skip_next_compile = GL_TRUE;
        return;
    } else if (!*string) {
        l_error("<%p> Shader source *string is NULL, count: %i",
                   __builtin_return_address(0), count);
        skip_next_compile = GL_TRUE;
        return;
    }

    size_t total_length = 0;

    for (int i = 0; i < count; ++i) {
        total_length += (!_length || _length[i] < 0) ? strlen(string[i]) : (size_t)_length[i];
    }

    char * str = malloc(total_length+1);
    if (!str) fatal_error("Gangstar shader source allocation failed: %u bytes", (unsigned)total_length);
    size_t l = 0;

    for (int i = 0; i < count; ++i) {
        size_t part = (!_length || _length[i] < 0) ? strlen(string[i]) : (size_t)_length[i];
        memcpy(str + l, string[i], part);
        l += part;
    }
    str[total_length] = '\0';

#ifdef GANGSTAR_GRAPHICS_DIAGNOSTICS
    char *digest = str_sha1sum(str, total_length);
    if (digest) {
        char path[192];
        snprintf(path, sizeof(path), DATA_PATH "shader-diagnostics/%s.glsl", digest);
        if (!file_exists(path)) {
            sceIoMkdir(DATA_PATH "shader-diagnostics", 0777);
            file_save(path, (const uint8_t *)str, total_length);
        }
        GLint kind = 0;
        glGetShaderiv(shader, GL_SHADER_TYPE, &kind);
        l_info("Gangstar shader source: id=%u type=0x%x bytes=%u hash=%s", shader, kind, (unsigned)total_length, digest);
        free(digest);
    }
#endif
    load_shader(shader, str, total_length);

    free(str);
}

void glCompileShader_soloader(GLuint shader) {
#ifdef DEBUG_OPENGL
    sceClibPrintf("[gl_dbg] glCompileShader<%p>(shader: %i)\n", __builtin_return_address(0), shader);
#endif

#ifndef USE_GXP_SHADERS
    if (!skip_next_compile) {
        glCompileShader(shader);
        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            char diagnostic[2048] = {0};
            glGetShaderInfoLog(shader, sizeof(diagnostic), NULL, diagnostic);
            l_error("Gangstar shader %u compilation failed: %s", shader, diagnostic);
        }
#ifdef DUMP_COMPILED_SHADERS
        void *bin = vglMalloc(32 * 1024);
        GLsizei len;
        vglGetShaderBinary(shader, 32 * 1024, &len, bin);
        file_save(next_shader_fname, bin, len);
        vglFree(bin);
#endif
    }
    skip_next_compile = GL_FALSE;
#endif
}

#if defined(USE_GLSL_SHADERS) && defined(DUMP_COMPILED_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char gxp_path[256];
    snprintf(gxp_path, sizeof(gxp_path), DATA_PATH"gxp/%s.gxp", sha_name);

    if (file_exists(gxp_path)) {
        uint8_t *buffer;
        size_t size;

        file_load(gxp_path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
        skip_next_compile = GL_TRUE;
    } else {
        glShaderSource(shader, 1, &string, &length);
        strcpy(next_shader_fname, gxp_path);
    }

    free(sha_name);
}
#elif defined(USE_GLSL_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    glShaderSource(shader, 1, &string, &length);
}
#elif defined(USE_CG_SHADERS) && defined(DUMP_COMPILED_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char gxp_path[256];
    char cg_path[256];
    snprintf(gxp_path, sizeof(gxp_path), DATA_PATH"gxp/%s.gxp", sha_name);
    snprintf(cg_path, sizeof(cg_path), DATA_PATH"cg/%s.cg", sha_name);

    if (file_exists(gxp_path)) {
        uint8_t *buffer;
        size_t size;

        file_load(gxp_path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
        skip_next_compile = GL_TRUE;
    } else if (file_exists(cg_path)) {
        char *buffer;
        size_t size;

        file_load(cg_path, (uint8_t **) &buffer, &size);

        glShaderSource(shader, 1, &string, &size);
        strcpy(next_shader_fname, gxp_path);

        free(buffer);
        skip_next_compile = GL_FALSE;
    } else {
        l_warn("Encountered an untranslated shader %s, saving GLSL "
               "and using a dummy shader.", sha_name);

        char glsl_path[256];
        snprintf(glsl_path, sizeof(glsl_path), DATA_PATH"glsl/%s.glsl", sha_name);
        file_mkpath(glsl_path, 0777);
        file_save(glsl_path, (const uint8_t *) string, length);

        if (strstr(string, "gl_FragColor")) {
            const char *dummy_shader = "float4 main() { return float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        } else {
            const char *dummy_shader = "void main(float4 out gl_Position : POSITION ) { gl_Position = float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        }

        skip_next_compile = GL_FALSE;
    }

    free(sha_name);
}
#elif defined(USE_CG_SHADERS) || defined(USE_GXP_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char path[256];
#ifdef USE_CG_SHADERS
    snprintf(path, sizeof(path), DATA_PATH"cg/%s.cg", sha_name);
#else
    snprintf(path, sizeof(path), DATA_PATH"gxp/%s.gxp", sha_name);
#endif

    if (file_exists(path)) {
#ifdef USE_CG_SHADERS
        char *buffer;
        size_t size;

        file_load(path, (uint8_t **) &buffer, &size);

        glShaderSource(shader, 1, &string, &size);

        free(buffer);
#else
        uint8_t *buffer;
        size_t size;

        file_load(path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
#endif
    } else {
        l_warn("Encountered an untranslated shader %s, saving GLSL "
               "and using a dummy shader.", sha_name);

        char glsl_path[256];
        snprintf(glsl_path, sizeof(glsl_path), DATA_PATH"glsl/%s.glsl", sha_name);
        file_mkpath(glsl_path, 0777);
        file_save(glsl_path, (const uint8_t *) string, length);

        if (strstr(string, "gl_FragColor")) {
            const char *dummy_shader = "float4 main() { return float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        } else {
            const char *dummy_shader = "void main(float4 out gl_Position : POSITION ) { gl_Position = float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        }
    }

    free(sha_name);
}
#else
#error "Define one of (USE_GLSL_SHADERS, USE_CG_SHADERS, USE_GXP_SHADERS)"
#endif


void glLinkProgram_soloader(GLuint program) {
    void *recipe = gangstar_shader_capture(program);
    uint64_t started = sceKernelGetProcessTimeWide();
    glLinkProgram(program);
    uint64_t elapsed = sceKernelGetProcessTimeWide() - started;
    gangstar_asset_sample(RIO_ASSET_SHADER, elapsed, 0);
    if (elapsed >= 20000)
        l_info("Gangstar shader link stall: program=%u elapsed_us=%u", program, (unsigned)elapsed);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    gangstar_shader_capture_finish(recipe, linked);
    if (!linked) {
        char diagnostic[2048] = {0};
        glGetProgramInfoLog(program, sizeof(diagnostic), NULL, diagnostic);
        l_error("Gangstar program %u link failed: %s", program, diagnostic);
        return;
    }
#ifdef GANGSTAR_GRAPHICS_DIAGNOSTICS
    GLint attributes = 0;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attributes);
    l_info("Gangstar program linked: id=%u attributes=%d", program, attributes);
    for (GLint i = 0; i < attributes && i < 16; ++i) {
        char name[128] = {0};
        GLint size = 0;
        GLenum type = 0;
        glGetActiveAttrib(program, i, sizeof(name), NULL, &size, &type, name);
        l_info("Gangstar attribute: program=%u location=%d type=0x%x size=%d name=%s",
            program, glGetAttribLocation(program, name), type, size, name);
    }
#endif
}

void glVertexAttribPointer_soloader(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
#ifdef GANGSTAR_GRAPHICS_DIAGNOSTICS
    struct Format { GLuint index; GLint size; GLenum type; GLboolean normalized; GLsizei stride; };
    static struct Format formats[64];
    static unsigned count;
    for (unsigned i = 0; i < count; ++i) {
        struct Format *f = &formats[i];
        if (f->index == index && f->size == size && f->type == type && f->normalized == normalized && f->stride == stride) return;
    }
    if (count == 64) return;
    formats[count++] = (struct Format){index, size, type, normalized, stride};
    GLint buffer = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    l_info("Gangstar vertex format: index=%u size=%d type=0x%x normalized=%u stride=%d buffer=0x%x pointer=%p", index, size, type, normalized, stride, buffer, pointer);
#endif
}


// On-demand, one-frame evidence. Retain only observed API inputs, never change
// native uniforms or vertex data to manufacture an apparently correct HUD.
#ifdef GANGSTAR_GRAPHICS_DIAGNOSTICS
#include <so_util/so_util.h>
extern so_module so_mod;
#define HUD_UNIFORMS 2048
struct HudUniform { GLint location; GLuint program; char name[80]; GLfloat value[16]; int values, count, transpose, occupied; };
static struct HudUniform hud_uniforms[HUD_UNIFORMS];
static struct HudUniform *hud_uniform(GLint location, int create) {
    if (location == -1) return NULL;
    unsigned slot = ((unsigned)location * 2654435761u) % HUD_UNIFORMS;
    for (unsigned i = 0; i < HUD_UNIFORMS; ++i, slot = (slot + 1) % HUD_UNIFORMS) {
        struct HudUniform *entry = &hud_uniforms[slot];
        if (entry->occupied && entry->location == location) return entry;
        if (!entry->occupied) {
            if (!create) return NULL;
            entry->occupied = 1; entry->location = location; return entry;
        }
    }
    return NULL;
}
GLint glGetUniformLocation_soloader(GLuint program, const GLchar *name) {
    GLint location = glGetUniformLocation(program, name);
    if (name && (strstr(name, "Matrix") || strstr(name, "matrix") || strstr(name, "scaleoffset") || strstr(name, "Sampler") || strstr(name, "texture") || strstr(name, "Color"))) {
        struct HudUniform *entry = hud_uniform(location, 1);
        if (entry) {
            entry->program = program;
            snprintf(entry->name, sizeof(entry->name), "%s", name);
            entry->values = 0;
        }
    }
    return location;
}
static void hud_values(GLint location, int count, int transpose, const GLfloat *value, int values) {
    struct HudUniform *entry = hud_uniform(location, 0);
    if (entry && value && count > 0) {
        entry->count = count; entry->transpose = transpose; entry->values = values;
        memcpy(entry->value, value, values * sizeof(GLfloat));
    }
}
void glUniformMatrix4fv_soloader(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    hud_values(location, count, transpose, value, 16);
    glUniformMatrix4fv(location, count, transpose, value);
}
void glUniform4fv_soloader(GLint location, GLsizei count, const GLfloat *value) {
    hud_values(location, count, 0, value, 4);
    glUniform4fv(location, count, value);
}
void glUniform1i_soloader(GLint location, GLint value) {
    GLfloat input = value; hud_values(location, 1, 0, &input, 1);
    glUniform1i(location, value);
}
extern void vglDebugProgramState(GLuint program, void *output);
extern void vglSetDebugDrawOutput(void *output);
static FILE *hud_trace;
static unsigned hud_poll, hud_draws;
static GLuint hud_dumped[32];
static unsigned hud_dumped_count;
void gangstar_hud_trace_poll(void) {
    if (hud_trace) {
        fclose(hud_trace); hud_trace = NULL;
        l_info("Gangstar HUD trace completed: draws=%u", hud_draws);
        return;
    }
    if (++hud_poll % 60) return;
    FILE *request = fopen(DATA_PATH "hud-trace.request", "rb");
    if (!request) return;
    fclose(request); remove(DATA_PATH "hud-trace.request");
    hud_trace = fopen(DATA_PATH "hud-trace.log", "wb");
    hud_draws = 0; hud_dumped_count = 0;
    sceIoMkdir(DATA_PATH "hud-textures", 0777);
}
static void hud_trace_draw(GLenum mode, GLint first, GLsizei count, GLenum index_type, const void *indices) {
    if (!hud_trace || hud_draws++ >= 256) return;
    GLint program = 0, framebuffer = 0, active = 0, elements = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elements);
    fprintf(hud_trace, "draw=%u program=%d framebuffer=0x%x mode=0x%x count=%d first=%d index=0x%x ebo=0x%x\n", hud_draws, program, framebuffer, mode, count, first, index_type, elements);
    // Compare the engine's cached objects with actual GL bindings. Layout
    // verified against the supported ELF's CCommonGLDriver::setTexture.
    // E_TEXTURE_TYPE 1 is GL_TEXTURE_2D; category 0 is GL_TEXTURE_1D.
    static uintptr_t (*get_driver)(void);
    if (!get_driver) get_driver = (void *)so_symbol(&so_mod, "_Z14GetVideoDriverv");
    uintptr_t driver = get_driver ? get_driver() : 0;
    for (int unit = 0; unit < 8; ++unit) {
        GLint texture = 0; glActiveTexture(GL_TEXTURE0 + unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        fprintf(hud_trace, " tex%d=0x%x", unit, texture);
        if (driver) {
            uintptr_t object = *(uintptr_t *)(driver + 0x43c + unit * 4);
            fprintf(hud_trace, "[native=%p", (void *)object);
            if (object) fprintf(hud_trace, " gl=%u flags=%u size=%ux%u name=%s",
                *(unsigned *)(object + 0x60), *(unsigned char *)(object + 0x38),
                *(unsigned *)(object + 0x18), *(unsigned *)(object + 0x1c),
                *(const char **)(object + 8) ? *(const char **)(object + 8) : "(null)");
            fprintf(hud_trace, "]");
        }
        if (texture && program > 0) {
            SceGxmTexture *descriptor = vglGetGxmTexture(GL_TEXTURE_2D);
            fprintf(hud_trace, "(%ux%u format=0x%x type=0x%x data=%p)", sceGxmTextureGetWidth(descriptor), sceGxmTextureGetHeight(descriptor),
                sceGxmTextureGetFormat(descriptor), sceGxmTextureGetType(descriptor), vglGetTexDataPointer(GL_TEXTURE_2D));
            fprintf(hud_trace, "(min=%u mag=%u mipfilter=%u levels=%u lod=%u)", sceGxmTextureGetMinFilter(descriptor), sceGxmTextureGetMagFilter(descriptor),
                sceGxmTextureGetMipFilter(descriptor), sceGxmTextureGetMipmapCount(descriptor), sceGxmTextureGetLodBias(descriptor));
            if (vglGetTexDataPointer(GL_TEXTURE_2D) && sceGxmTextureGetFormat(descriptor) == SCE_GXM_TEXTURE_FORMAT_ETC1_1BGR && hud_dumped_count < 32) {
                unsigned seen = 0;
                for (unsigned k = 0; k < hud_dumped_count; ++k) if (hud_dumped[k] == (GLuint)texture) seen = 1;
                if (!seen) {
                    hud_dumped[hud_dumped_count++] = texture;
                    unsigned width = sceGxmTextureGetWidth(descriptor), height = sceGxmTextureGetHeight(descriptor);
                    char path[160]; snprintf(path, sizeof(path), DATA_PATH "hud-textures/tex-%u-%u-%u.etc1", texture, width, height);
                    FILE *output = fopen(path, "wb");
                    if (output) { fwrite(vglGetTexDataPointer(GL_TEXTURE_2D), 1, ((width+3)/4)*((height+3)/4)*8, output); fclose(output); }
                }
            }
        }
    }
    glActiveTexture(active); fprintf(hud_trace, "\n");
    for (unsigned i = 0; i < HUD_UNIFORMS; ++i) {
        struct HudUniform *entry = &hud_uniforms[i];
        if (!entry->occupied || entry->program != (GLuint)program) continue;
        fprintf(hud_trace, " uniform %s count=%d transpose=%d", entry->name, entry->count, entry->transpose);
        for (int j = 0; j < entry->values; ++j) fprintf(hud_trace, " %.7g", entry->value[j]);
        fprintf(hud_trace, "\n");
    }
    if (program <= 0) return;
    vglDebugProgramState(program, hud_trace);
    const char *names[] = {"Vertex", "Position", "TexCoord0", "texcoord0", "Texcoord0", "Color0"};
    for (unsigned n = 0; n < sizeof(names)/sizeof(names[0]); ++n) {
        GLint location = glGetAttribLocation(program, names[n]);
        if (location < 0) continue;
        GLint enabled = 0, buffer = 0, size = 0, stride = 0, type = 0;
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
        if (!enabled) continue;
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
        void *pointer = NULL; glGetVertexAttribPointerv(location, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
        fprintf(hud_trace, " attribute %s loc=%d buffer=0x%x size=%d stride=%d type=0x%x pointer=%p", names[n], location, buffer, size, stride, type, pointer);
        if (!buffer && pointer && size > 0 && size <= 4 && (type == GL_FLOAT || type == GL_SHORT || type == GL_UNSIGNED_BYTE) && (!index_type || !elements)) {
            int component = type == GL_FLOAT ? 4 : type == GL_SHORT ? 2 : 1;
            if (!stride) stride = size * component;
            for (int vertex = 0; vertex < 3 && vertex < count; ++vertex) {
                unsigned index = first + vertex;
                if (index_type && indices) {
                    if (index_type == GL_UNSIGNED_SHORT) index = ((const uint16_t *)indices)[vertex];
                    else if (index_type == GL_UNSIGNED_BYTE) index = ((const uint8_t *)indices)[vertex];
                    else if (index_type == GL_UNSIGNED_INT) index = ((const uint32_t *)indices)[vertex];
                    else break;
                }
                fprintf(hud_trace, " [%u:", index);
                const uint8_t *data = (const uint8_t *)pointer + index * stride;
                for (int j = 0; j < size; ++j) {
                    float value;
                    if (type == GL_FLOAT) memcpy(&value, data + j * 4, 4);
                    else if (type == GL_SHORT) { int16_t v; memcpy(&v, data + j * 2, 2); value = v; }
                    else value = data[j];
                    fprintf(hud_trace, " %.7g", value);
                }
                fprintf(hud_trace, "]");
            }
        }
        fprintf(hud_trace, "\n");
    }
}
void glDrawArrays_soloader(GLenum mode, GLint first, GLsizei count) {
    hud_trace_draw(mode, first, count, 0, NULL); glDrawArrays(mode, first, count);
}
void glDrawElements_soloader(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    hud_trace_draw(mode, 0, count, type, indices);
    GLint program = 0;
    if (hud_trace) glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    if (hud_trace && program > 0 && program <= 19) vglSetDebugDrawOutput(hud_trace);
    glDrawElements(mode, count, type, indices);
    vglSetDebugDrawOutput(NULL);
}
#else
GLint glGetUniformLocation_soloader(GLuint program, const GLchar *name) {
    return glGetUniformLocation(program, name);
}
void glUniformMatrix4fv_soloader(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    glUniformMatrix4fv(location, count, transpose, value);
}
void glUniform4fv_soloader(GLint location, GLsizei count, const GLfloat *value) {
    glUniform4fv(location, count, value);
}
void glUniform1i_soloader(GLint location, GLint value) {
    glUniform1i(location, value);
}
void gangstar_hud_trace_poll(void) {}
void glDrawArrays_soloader(GLenum mode, GLint first, GLsizei count) {
    glDrawArrays(mode, first, count);
}
void glDrawElements_soloader(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    glDrawElements(mode, count, type, indices);
}
#endif

