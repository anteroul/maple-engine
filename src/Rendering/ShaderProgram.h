#ifndef MAPLEENGINE_SHADERPROGRAM_H
#define MAPLEENGINE_SHADERPROGRAM_H

#include <GL/glew.h>

/// Compile and link a shader program from in-memory sources. Prints the
/// compiler's log and returns 0 on failure. `name` only labels the messages.
GLuint linkProgram(const char* vertexSource, const char* fragmentSource, const char* name);

#endif //MAPLEENGINE_SHADERPROGRAM_H
