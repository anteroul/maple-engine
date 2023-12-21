#ifndef MAPLEENGINE_SHADER_H
#define MAPLEENGINE_SHADER_H

#include "../Component.h"

class Shader : public Component {
public:
    Shader(Entity &owner, const std::string &path);
};


#endif //MAPLEENGINE_SHADER_H
