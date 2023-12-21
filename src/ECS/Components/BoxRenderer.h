#ifndef MAPLEENGINE_BOXRENDERER_H
#define MAPLEENGINE_BOXRENDERER_H

#include "../Component.h"
#include <glm/glm.hpp>

class BoxRenderer : public Component {
public:
    BoxRenderer(Entity& owner, glm::vec3 colour);
    ~BoxRenderer() override;
    void render() const override;
private:
    b2Vec2 m_Size;
    glm::vec3 m_Colour{};
};


#endif //MAPLEENGINE_BOXRENDERER_H
