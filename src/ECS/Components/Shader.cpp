#include "Shader.h"
#include "../../Log/msg.h"
#include <utility>
#include <fstream>
#include <sstream>
#include <GL/glew.h>

Shader::Shader(Entity &owner, std::string path)
    : Component(owner), m_FilePath(std::move(path)), m_RendererID(0)
{}

void Shader::initialize()
{
    compileShader();
}

Shader::~Shader()
{
    glDeleteProgram(m_RendererID);
}

void Shader::bind() const
{
    glUseProgram(m_RendererID);
}

void Shader::unbind() const
{
    glUseProgram(0);
}

void Shader::render() const
{
    bind();
}

Shader::ShaderProgramSource Shader::parseShader(const std::string &filepath)
{
    std::ifstream stream(filepath);
    if (!stream.is_open())
    {
        printf("%s \bERROR: Shader file not found at: %s", WARNING, filepath.c_str()); // adjust to your msg.h API
        return {"", ""};
    }

    enum class ShaderType
    {
        NONE = -1,
        VERTEX = 0,
        FRAGMENT = 1
    };

    std::string line;
    std::stringstream ss[2];
    ShaderType type = ShaderType::NONE;

    while (getline(stream, line))
    {
        if (line.find("#shader") != std::string::npos)
        {
            if (line.find("vertex") != std::string::npos)
                type = ShaderType::VERTEX;
            else if (line.find("fragment") != std::string::npos)
                type = ShaderType::FRAGMENT;
        }
        else if (type != ShaderType::NONE)
        {
            ss[(int)type] << line << '\n';
        }
    }

    return {ss[0].str(), ss[1].str()};
}

unsigned int Shader::compileSingleShader(unsigned int type, const std::string &source)
{
    unsigned int id = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE)
    {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        std::string message(length, ' ');
        glGetShaderInfoLog(id, length, &length, message.data());

        printf("%s \bERROR: %s", WARNING, std::string("Failed to compile " +
                std::string(type == GL_VERTEX_SHADER ? "vertex" : "fragment") +
                " shader: " + message).c_str());

        glDeleteShader(id);
        return 0;
    }

    return id;
}

bool Shader::compileShader()
{
    ShaderProgramSource source = parseShader(m_FilePath);
    if (source.vertexSource.empty() || source.fragmentSource.empty())
        return false;

    unsigned int program = glCreateProgram();
    unsigned int vs = compileSingleShader(GL_VERTEX_SHADER, source.vertexSource);
    unsigned int fs = compileSingleShader(GL_FRAGMENT_SHADER, source.fragmentSource);

    if (vs == 0 || fs == 0)
    {
        glDeleteProgram(program);
        return false;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE)
    {
        int length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string message(length, ' ');
        glGetProgramInfoLog(program, length, &length, message.data());
        printf("%s \bERROR: Shader link error: %s\n", WARNING, message.c_str());

        glDeleteProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    glValidateProgram(program);

    // Once linked, the individual shader objects aren't needed anymore.
    glDeleteShader(vs);
    glDeleteShader(fs);

    m_RendererID = program;
    return true;
}

int Shader::getUniformLocation(const std::string& name) const
{
    auto it = m_UniformLocationCache.find(name);
    if (it != m_UniformLocationCache.end())
        return it->second;

    int location = glGetUniformLocation(m_RendererID, name.c_str());
    if (location == -1)
        printf("%s \bWARNING: Uniform '%s' doesn't exist or is unused (optimized out)\n", WARNING, name.c_str());

    m_UniformLocationCache[name] = location;
    return location;
}

void Shader::setUniform1i(const std::string &name, int value) const
{
    glUniform1i(getUniformLocation(name), value);
}

void Shader::setUniform1f(const std::string &name, float value) const
{
    glUniform1f(getUniformLocation(name), value);
}

void Shader::setUniform4f(const std::string &name, float v0, float v1, float v2, float v3) const
{
    glUniform4f(getUniformLocation(name), v0, v1, v2, v3);
}

void Shader::setUniformMat4f(const std::string &name, const float *matrix) const
{
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, matrix);
}