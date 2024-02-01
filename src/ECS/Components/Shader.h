#ifndef MAPLEENGINE_SHADER_H
#define MAPLEENGINE_SHADER_H

#include "../Component.h"
#include "../../Log/msg.h"
#include <cstdio>

class Shader : public Component {
public:
    Shader(Entity &owner, const std::string &path);
    void render() const override;
private:
    GLint* m_Shader;
};


#endif //MAPLEENGINE_SHADER_H
