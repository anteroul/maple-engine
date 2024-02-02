#ifndef MAPLEENGINE_SHADER_H
#define MAPLEENGINE_SHADER_H

#include "../Component.h"
#include "../../Log/msg.h"

// TODO: Shader class implementation
class Shader : public Component {
private:
    std::string m_FilePath;
    unsigned int m_RendererID;
public:
    Shader(Entity &owner, std::string  path);
    ~Shader() override;
    void render() const override;
    void bind() const;
    void unbind() const;

    // Set uniforms
    void setUniform4f(const std::string& name, float v0, float v1, float f2, float f3);
private:
    bool compileShader();
    unsigned int getUniformLocation(const std::string& name);
};


#endif //MAPLEENGINE_SHADER_H
