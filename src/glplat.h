// Platform OpenGL header. macOS ships full core-profile declarations;
// Windows opengl32 only exports GL 1.1, so everything the app uses is loaded
// at runtime through glfwGetProcAddress (which falls back to opengl32.dll
// for 1.1 entry points). Call glplatInit() once after the first GL context
// is current. Build with GLFW_INCLUDE_NONE on Windows so these declarations
// are the only ones.
#pragma once

#ifdef __APPLE__
#include <OpenGL/gl3.h>
static inline void glplatInit() {}
#else

#include <cstddef>

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef float GLclampf;
typedef char GLchar;
typedef void GLvoid;
typedef ptrdiff_t GLsizeiptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_BLEND 0x0BE2
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_RED 0x1903
#define GL_R8 0x8229
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE0 0x84C0
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0

#ifndef GLPLAT_APIENTRY
#define GLPLAT_APIENTRY __stdcall
#endif

#define GLPLAT_FUNCS(X)                                                        \
    X(void, glActiveTexture, (GLenum))                                         \
    X(void, glAttachShader, (GLuint, GLuint))                                  \
    X(void, glBindFramebuffer, (GLenum, GLuint))                               \
    X(void, glBindTexture, (GLenum, GLuint))                                   \
    X(void, glBindVertexArray, (GLuint))                                       \
    X(void, glBlitFramebuffer,                                                 \
      (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield,     \
       GLenum))                                                                \
    X(void, glClear, (GLbitfield))                                             \
    X(void, glClearColor, (GLclampf, GLclampf, GLclampf, GLclampf))            \
    X(void, glCompileShader, (GLuint))                                         \
    X(GLuint, glCreateProgram, (void))                                         \
    X(GLuint, glCreateShader, (GLenum))                                        \
    X(void, glDeleteFramebuffers, (GLsizei, const GLuint*))                    \
    X(void, glDeleteProgram, (GLuint))                                         \
    X(void, glDeleteShader, (GLuint))                                          \
    X(void, glDeleteTextures, (GLsizei, const GLuint*))                        \
    X(void, glDisable, (GLenum))                                               \
    X(void, glDrawArrays, (GLenum, GLint, GLsizei))                            \
    X(void, glEnable, (GLenum))                                                \
    X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))   \
    X(void, glGenFramebuffers, (GLsizei, GLuint*))                             \
    X(void, glGenTextures, (GLsizei, GLuint*))                                 \
    X(void, glGenVertexArrays, (GLsizei, GLuint*))                             \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))         \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint*))                          \
    X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))          \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint*))                           \
    X(void, glGetTexImage, (GLenum, GLint, GLenum, GLenum, GLvoid*))           \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar*))                    \
    X(void, glLinkProgram, (GLuint))                                           \
    X(void, glShaderSource,                                                    \
      (GLuint, GLsizei, const GLchar* const*, const GLint*))                   \
    X(void, glTexImage2D,                                                      \
      (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,          \
       const GLvoid*))                                                         \
    X(void, glTexParameteri, (GLenum, GLenum, GLint))                          \
    X(void, glTexSubImage2D,                                                   \
      (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,          \
       const GLvoid*))                                                         \
    X(void, glUniform1f, (GLint, GLfloat))                                     \
    X(void, glUniform1i, (GLint, GLint))                                       \
    X(void, glUniform2f, (GLint, GLfloat, GLfloat))                            \
    X(void, glUniform2fv, (GLint, GLsizei, const GLfloat*))                    \
    X(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat))                   \
    X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))          \
    X(void, glUniform4fv, (GLint, GLsizei, const GLfloat*))                    \
    X(void, glUseProgram, (GLuint))                                            \
    X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))

#define GLPLAT_DECL(ret, name, args) extern ret(GLPLAT_APIENTRY* name) args;
GLPLAT_FUNCS(GLPLAT_DECL)
#undef GLPLAT_DECL

void glplatInit();

#endif // !__APPLE__
