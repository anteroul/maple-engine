#ifndef MAPLEENGINE_SHADERPROGRAM_H
#define MAPLEENGINE_SHADERPROGRAM_H

#include <GL/glew.h>

/// Compile and link a shader program from in-memory sources. Prints the
/// compiler's log and returns 0 on failure. `name` only labels the messages.
GLuint linkProgram(const char* vertexSource, const char* fragmentSource, const char* name);

/// Compile and link a compute shader. `defines` is inserted right after the
/// #version line, for constants the C++ side has to agree on. Returns 0 on
/// failure, like linkProgram.
GLuint linkCompute(const char* source, const char* name, const char* defines = "");

/// Whether compute shaders can be used: OpenGL 4.3, or the extensions that
/// make it up. Needs a current context. The first time it says no, it prints
/// why, once. Setting MAPLE_NO_COMPUTE=1 in the environment forces it to say
/// no, so the CPU fallbacks can be tried on any machine.
bool computeSupported();

/// Whether the GL context is gone. Objects that own GL resources and may be
/// destroyed after it, such as components of entities still alive at exit,
/// check this before deleting them.
bool renderingReleased();
void markRenderingReleased();

#endif //MAPLEENGINE_SHADERPROGRAM_H
