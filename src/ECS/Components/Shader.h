#ifndef MAPLEENGINE_SHADER_H
#define MAPLEENGINE_SHADER_H

#include <string>
#include <unordered_map>
#include "../Component.h"
#include "../../Log/msg.h"

class Shader : public Component
{
private:
    std::string m_FilePath;
    unsigned int m_RendererID;
    mutable std::unordered_map<std::string, int> m_UniformLocationCache;

    struct ShaderProgramSource
    {
        std::string vertexSource;
        std::string fragmentSource;
    };

public:
    Shader(Entity &owner, std::string path);
    ~Shader() override;

    void initialize() override;
    void render() const override;
    void bind() const;
    void unbind() const;

    void setUniform1i(const std::string &name, int value) const;
    void setUniform1f(const std::string &name, float value) const;
    void setUniform4f(const std::string &name, float v0, float v1, float v2, float v3) const;
    void setUniformMat4f(const std::string &name, const float *matrix) const;

private:
    bool compileShader();
    ShaderProgramSource parseShader(const std::string &filepath);
    unsigned int compileSingleShader(unsigned int type, const std::string &source);
    int getUniformLocation(const std::string &name) const;
};

#endif // MAPLEENGINE_SHADER_H