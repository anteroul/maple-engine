#ifndef MAPLEENGINE_SPHERERENDERER_H
#define MAPLEENGINE_SPHERERENDERER_H

#include "../Component.h"
#include "../Rendering/MeshGPU.h"
#include "../Geometry/SphereGeometry.h"
#include "Shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>

class SphereRenderer : public Component {
private:
    MeshGPUHandle m_Handle;
    float m_Radius;
    int m_Sectors, m_Stacks;
public:
    SphereRenderer(Entity& owner, float radius = 1.0f, int sectors = 32, int stacks = 16)
        : Component(owner), m_Radius(radius), m_Sectors(sectors), m_Stacks(stacks) { initialize(); }
    
    ~SphereRenderer() override { destroyMesh(m_Handle); }
    
    void initialize() override
    {
        MeshData mesh = generateSphereGeometry(m_Radius, m_Sectors, m_Stacks);
        m_Handle = uploadMesh(mesh);
    }

    void render() const override
    {
        Shader* shader = getComponent<Shader>();
        if (!shader) {
            renderLegacy();
            return;
        } else {
            shader->bind();
            drawMesh(m_Handle);
            shader->unbind();
        }
    }    
protected:
    void renderLegacy() const;
    glm::vec3 m_Colour{};
    GLUquadric *m_Quadric;
};


#endif //MAPLEENGINE_SPHERERENDERER_H
