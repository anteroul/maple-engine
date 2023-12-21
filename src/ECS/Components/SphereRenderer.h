#ifndef MAPLEENGINE_SPHERERENDERER_H
#define MAPLEENGINE_SPHERERENDERER_H

#include "../Component.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

class SphereRenderer : public Component {
public:
    SphereRenderer(Entity& owner, glm::vec3 colour);
    void render() const override;
    float m_Radius;
private:
    glm::vec3 m_Colour{};
    GLUquadric *m_Quadric;
};


#endif //MAPLEENGINE_SPHERERENDERER_H
