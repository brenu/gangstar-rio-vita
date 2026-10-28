#include "gangstar_shader_warmup.h"
#include "utils/utils.h"
#include "utils/logger.h"
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define RECIPE_DIR "ux0:/data/gangstarrio/shader-warmup-v1"
#define SOURCE_LIMIT 65536u
#define PAIR_LIMIT 256u
typedef struct { uint32_t magic, vertex, fragment; } RecipeHeader;
#define RECIPE_MAGIC 0x31575352u
typedef struct { RecipeHeader h; char source[]; } Recipe;

// Capture the exact GLSL pair before VitaGL consumes/frees source at link.
// Already compiled/reused shaders may no longer expose source; skip those pairs.
void *gangstar_shader_capture(GLuint program) {
#ifdef GANGSTAR_DISABLE_SHADER_WARMUP
    (void)program; return NULL;
#endif
    GLuint shaders[2]; GLsizei count = 0;
    glGetAttachedShaders(program, 2, &count, shaders);
    if (count != 2) return NULL;
    GLuint vertex = 0, fragment = 0;
    GLint vl = 0, fl = 0;
    for (unsigned i = 0; i < 2; ++i) {
        GLint type = 0; glGetShaderiv(shaders[i], GL_SHADER_TYPE, &type);
        if (type == GL_VERTEX_SHADER) vertex = shaders[i];
        if (type == GL_FRAGMENT_SHADER) fragment = shaders[i];
    }
    if (!vertex || !fragment) return NULL;
    glGetShaderiv(vertex, GL_SHADER_SOURCE_LENGTH, &vl);
    glGetShaderiv(fragment, GL_SHADER_SOURCE_LENGTH, &fl);
    if (vl <= 1 || fl <= 1 || (unsigned)vl > SOURCE_LIMIT || (unsigned)fl > SOURCE_LIMIT) return NULL;
    Recipe *r = calloc(1, sizeof(*r) + (size_t)vl + fl);
    if (!r) return NULL;
    r->h = (RecipeHeader){RECIPE_MAGIC, (uint32_t)vl, (uint32_t)fl};
    glGetShaderSource(vertex, vl, NULL, r->source);
    glGetShaderSource(fragment, fl, NULL, r->source + vl);
    return r;
}

void gangstar_shader_capture_finish(void *recipe, int linked) {
    Recipe *r = recipe;
    if (!r) return;
    if (linked) {
        size_t size = sizeof(*r) + r->h.vertex + r->h.fragment;
        char *hash = str_sha1sum((const char *)r, size);
        if (hash) {
            char path[192], temporary[200];
            snprintf(path, sizeof(path), RECIPE_DIR "/%s.rsw", hash);
            snprintf(temporary, sizeof(temporary), "%s.tmp", path);
            free(hash);
            if (!file_exists(path)) {
                sceIoMkdir(RECIPE_DIR, 0777);
                FILE *f = fopen(temporary, "wb");
                if (f) {
                    int ok = fwrite(r, 1, size, f) == size;
                    if (fclose(f)) ok = 0;
                    if (ok) { if (rename(temporary, path)) remove(temporary); }
                    else remove(temporary);
                }
            }
        }
    }
    free(r);
}

static Recipe *read_recipe(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    RecipeHeader h;
    if (fread(&h, 1, sizeof(h), f) != sizeof(h) || h.magic != RECIPE_MAGIC ||
        h.vertex < 2 || h.fragment < 2 || h.vertex > SOURCE_LIMIT || h.fragment > SOURCE_LIMIT) {
        fclose(f); return NULL;
    }
    size_t bytes = (size_t)h.vertex + h.fragment;
    Recipe *r = malloc(sizeof(*r) + bytes);
    if (r) {
        r->h = h;
        if (fread(r->source, 1, bytes, f) != bytes || fgetc(f) != EOF ||
            r->source[h.vertex - 1] || r->source[bytes - 1]) { free(r); r = NULL; }
    }
    fclose(f);
    return r;
}

void gangstar_shader_warmup(void) {
#ifdef GANGSTAR_DISABLE_SHADER_WARMUP
    l_info("RIO_WARMUP disabled"); return;
#endif
    DIR *directory = opendir(RECIPE_DIR);
    if (!directory) { l_info("RIO_WARMUP no recorded pairs; learning during normal play"); return; }
    unsigned loaded = 0, failed = 0, seen = 0;
    uint64_t started = sceKernelGetProcessTimeWide();
    struct dirent *entry;
    while (seen < PAIR_LIMIT && (entry = readdir(directory))) {
        size_t n = strlen(entry->d_name);
        if (n != 44 || strcmp(entry->d_name + 40, ".rsw")) continue;
        int valid_name = 1;
        for (unsigned i = 0; i < 40; ++i)
            if (!((entry->d_name[i] >= '0' && entry->d_name[i] <= '9') ||
                (entry->d_name[i] >= 'a' && entry->d_name[i] <= 'f') ||
                (entry->d_name[i] >= 'A' && entry->d_name[i] <= 'F'))) valid_name = 0;
        if (!valid_name) continue;
        ++seen;
        char path[192]; snprintf(path, sizeof(path), RECIPE_DIR "/%s", entry->d_name);
        Recipe *r = read_recipe(path);
        if (!r) { ++failed; continue; }
        GLuint vertex = glCreateShader(GL_VERTEX_SHADER), fragment = glCreateShader(GL_FRAGMENT_SHADER);
        GLuint program = glCreateProgram();
        if (vertex && fragment && program) {
            const GLchar *vs = r->source, *fs = r->source + r->h.vertex;
            glShaderSource(vertex, 1, &vs, NULL); glCompileShader(vertex);
            glShaderSource(fragment, 1, &fs, NULL); glCompileShader(fragment);
            glAttachShader(program, vertex); glAttachShader(program, fragment);
            glLinkProgram(program);
            GLint linked = 0; glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (linked) ++loaded; else ++failed;
        } else ++failed;
        if (program) glDeleteProgram(program);
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        free(r);
    }
    closedir(directory);
    (void)loaded; (void)failed; (void)started;
    l_info("RIO_WARMUP pairs=%u failed=%u elapsed_us=%llu", loaded, failed,
        (unsigned long long)(sceKernelGetProcessTimeWide() - started));
}
