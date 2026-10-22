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
    
    ~SphereRenderer() override
    {
        destroyMesh(m_Handle);
        if (m_Quadric) gluDeleteQuadric(m_Quadric);
    }

    /// Runs from the constructor and again from Entity::initialize(), so it
    /// must not leak the previous mesh or quadric.
    void initialize() override
    {
        destroyMesh(m_Handle);
        MeshData mesh = generateSphereGeometry(m_Radius, m_Sectors, m_Stacks);
        m_Handle = uploadMesh(mesh);

        if (!m_Quadric)
            m_Quadric = gluNewQuadric();
    }

    void setColour(const glm::vec3& colour) { m_Colour = colour; }

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
    glm::vec3 m_Colour{1.f, 1.f, 1.f};
    GLUquadric *m_Quadric = nullptr;
};


#endif //MAPLEENGINE_SPHERERENDERER_H
