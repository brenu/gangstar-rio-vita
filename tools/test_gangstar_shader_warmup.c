#include "gangstar_shader_warmup.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
static const char vertex[] = "void main(){gl_Position=vec4(1.0);}";
static const char fragment[] = "void main(){gl_FragColor=vec4(1.0);}";
static unsigned links, deletes, sources;
static const char path[] = "ux0:/data/gangstarrio/shader-warmup-v1/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.rsw";
uint64_t sceKernelGetProcessTimeWide(void) { return 100; }
int sceIoMkdir(const char *name, int mode) { return mkdir(name, mode); }
bool file_exists(const char *name) { return !access(name, F_OK); }
char *str_sha1sum(const char *bytes, size_t size) {
    assert(bytes && size > 12); return strdup("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
}
void glGetAttachedShaders(GLuint p, GLsizei n, GLsizei *count, GLuint *shaders) {
    assert(p == 3 && n == 2); *count = 2; shaders[0] = 1; shaders[1] = 2;
}
void glGetShaderiv(GLuint s, GLenum property, GLint *value) {
    assert(s == 1 || s == 2);
    if (property == GL_SHADER_TYPE) *value = s == 1 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
    else { assert(property == GL_SHADER_SOURCE_LENGTH); *value = s == 1 ? sizeof(vertex) : sizeof(fragment); }
}
void glGetShaderSource(GLuint s, GLsizei n, GLsizei *length, GLchar *out) {
    const char *source = s == 1 ? vertex : fragment;
    assert(n > 0 && (size_t)n == strlen(source) + 1); memcpy(out, source, strlen(source));
    if (length) *length = strlen(source); // Match VitaGL's non-terminated copy.
}
GLuint glCreateShader(GLenum t) { assert(t == GL_VERTEX_SHADER || t == GL_FRAGMENT_SHADER); return t == GL_VERTEX_SHADER ? 1 : 2; }
GLuint glCreateProgram(void) { return 3; }
void glShaderSource(GLuint s, GLsizei count, const GLchar *const *text, const GLint *length) {
    assert(count == 1 && !length && !strcmp(*text, s == 1 ? vertex : fragment)); ++sources;
}
void glCompileShader(GLuint s) { assert(s == 1 || s == 2); }
void glAttachShader(GLuint p, GLuint s) { assert(p == 3 && (s == 1 || s == 2)); }
void glLinkProgram(GLuint p) { assert(p == 3); ++links; }
void glGetProgramiv(GLuint p, GLenum property, GLint *value) { assert(p == 3 && property == GL_LINK_STATUS); *value = 1; }
void glDeleteProgram(GLuint p) { assert(p == 3); ++deletes; }
void glDeleteShader(GLuint s) { assert(s == 1 || s == 2); ++deletes; }
int main(void) {
    char root[] = "/tmp/rio-warmup-XXXXXX";
    char previous[4096]; assert(getcwd(previous, sizeof(previous)));
    assert(mkdtemp(root) && !chdir(root));
    gangstar_shader_warmup(); assert(!links); // Fresh install remains valid.
    assert(!mkdir("ux0:", 0700) && !mkdir("ux0:/data", 0700) && !mkdir("ux0:/data/gangstarrio", 0700));
    gangstar_shader_capture_finish(gangstar_shader_capture(3), 0); assert(!file_exists(path));
    gangstar_shader_capture_finish(gangstar_shader_capture(3), 1); assert(file_exists(path));
    gangstar_shader_warmup(); assert(links == 1 && sources == 2 && deletes == 3);
    FILE *f = fopen(path, "wb"); assert(f); fputs("corrupt", f); assert(!fclose(f));
    gangstar_shader_warmup(); assert(links == 1); // Bad cache never reaches compiler.
    uint32_t oversized[] = {0x31575352u, UINT32_MAX, UINT32_MAX};
    f = fopen(path, "wb"); assert(f);
    assert(fwrite(oversized, 1, sizeof(oversized), f) == sizeof(oversized)); assert(!fclose(f));
    gangstar_shader_warmup(); assert(links == 1);
    assert(!remove(path));
    assert(!rmdir("ux0:/data/gangstarrio/shader-warmup-v1") && !rmdir("ux0:/data/gangstarrio"));
    assert(!rmdir("ux0:/data") && !rmdir("ux0:"));
    assert(!chdir(previous) && !rmdir(root));
    puts("Shader warm-up: fresh install, successful-pair recording, exact replay, cleanup and corrupt-cache rejection passed");
}
