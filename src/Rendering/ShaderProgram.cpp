#include "ShaderProgram.h"
#include "../Log/msg.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

bool released = false;

const char* stageName(GLenum type)
{
    switch (type)
    {
        case GL_VERTEX_SHADER: return "vertex";
        case GL_FRAGMENT_SHADER: return "fragment";
        case GL_COMPUTE_SHADER: return "compute";
        default: return "unknown";
    }
}

/// Prints the link log and deletes the program if linking failed.
GLuint checkLinked(GLuint program, const char* name)
{
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

} // namespace

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
               stageName(type), message.c_str());
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
    return checkLinked(program, name);
}

GLuint linkCompute(const char* source, const char* name, const char* defines)
{
    // The #version line has to come first, so the defines go in after it.
    std::string text = source;
    const std::size_t lineEnd = text.find('\n', text.find("#version"));
    text.insert(lineEnd == std::string::npos ? text.size() : lineEnd + 1, defines);

    GLuint stage = compileStage(GL_COMPUTE_SHADER, text.c_str(), name);
    if (stage == 0)
        return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, stage);
    glLinkProgram(program);
    glDeleteShader(stage);
    return checkLinked(program, name);
}

bool computeSupported()
{
    static const bool supported = []
    {
        const char* reason = nullptr;
        const char* forced = std::getenv("MAPLE_NO_COMPUTE");
        if (forced && *forced && std::string(forced) != "0")
            reason = "MAPLE_NO_COMPUTE is set";
        else if (!GLEW_VERSION_4_3 && !(GLEW_ARB_compute_shader && GLEW_ARB_shader_storage_buffer_object
                                        && GLEW_ARB_shader_image_load_store))
            reason = "they need OpenGL 4.3";

        if (reason)
            printf("%s \bWARNING: Compute shaders are off, because %s. Particles and pixel "
                   "colours run on the CPU.\n", WARNING, reason);
        return reason == nullptr;
    }();
    return supported;
}

bool renderingReleased()
{
    return released;
}

void markRenderingReleased()
{
    released = true;
}
