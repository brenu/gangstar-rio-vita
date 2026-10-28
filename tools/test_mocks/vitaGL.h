#ifndef TEST_VITAGL_H
#define TEST_VITAGL_H
typedef unsigned GLenum;
typedef int GLint;
typedef int GLsizei;
typedef unsigned GLuint;
typedef char GLchar;
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_SHADER_TYPE 0x8B4F
#define GL_SHADER_SOURCE_LENGTH 0x8B88
#define GL_LINK_STATUS 0x8B82
void glGetAttachedShaders(GLuint, GLsizei, GLsizei *, GLuint *);
void glGetShaderiv(GLuint, GLenum, GLint *);
void glGetShaderSource(GLuint, GLsizei, GLsizei *, GLchar *);
GLuint glCreateShader(GLenum);
GLuint glCreateProgram(void);
void glShaderSource(GLuint, GLsizei, const GLchar *const *, const GLint *);
void glCompileShader(GLuint);
void glAttachShader(GLuint, GLuint);
void glLinkProgram(GLuint);
void glGetProgramiv(GLuint, GLenum, GLint *);
void glDeleteProgram(GLuint);
void glDeleteShader(GLuint);
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
void glCompressedTexImage2D(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
#endif

void glGenerateMipmap(GLenum);
