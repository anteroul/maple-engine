#include "ShaderProgram.h"
#include "../Log/msg.h"
#include <algorithm>
#include <cstdio>
#include <string>

static GLuint compileStage(GLenum type, const char* source, const char* name)
{
    GLuint stage = glCreateShader(type);
    glShaderSource(stage, 1, &source, nullptr);
    glCompileShader(stage);

    GLint status;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &length);
        std::string message(std::max(length, 1), '\0');
        glGetShaderInfoLog(stage, length, nullptr, message.data());
        printf("%s \bERROR: Failed to compile the %s %s shader: %s\n", WARNING, name,
               type == GL_VERTEX_SHADER ? "vertex" : "fragment", message.c_str());
        glDeleteShader(stage);
        return 0;
    }
    return stage;
}

GLuint linkProgram(const char* vertexSource, const char* fragmentSource, const char* name)
{
    GLuint vertex = compileStage(GL_VERTEX_SHADER, vertexSource, name);
    GLuint fragment = compileStage(GL_FRAGMENT_SHADER, fragmentSource, name);
    if (vertex == 0 || fragment == 0)
    {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string message(std::max(length, 1), '\0');
        glGetProgramInfoLog(program, length, nullptr, message.data());
        printf("%s \bERROR: Failed to link the %s shader: %s\n", WARNING, name, message.c_str());
        glDeleteProgram(program);
        return 0;
    }
    return program;
}
