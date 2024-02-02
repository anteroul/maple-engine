#include "Shader.h"
#include <utility>

// TODO: Shader class implementation
Shader::Shader(Entity &owner, std::string path) : Component(owner), m_FilePath(std::move(path)), m_RendererID(0)
{
    compileShader();
}

void Shader::render() const
{}

Shader::~Shader()
{}

void Shader::bind() const
{}

void Shader::unbind() const
{}

void Shader::setUniform4f(const std::string &name, float v0, float v1, float f2, float f3)
{}

bool Shader::compileShader()
{
    return false;
}

unsigned int Shader::getUniformLocation(const std::string &name)
{
    return 0;
}
