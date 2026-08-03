#ifndef MAPLEENGINE_BOXRENDERER_H
#define MAPLEENGINE_BOXRENDERER_H

#include "../Component.h"
#include "../Rendering/MeshGPU.h"
#include "../Geometry/BoxGeometry.h"
#include "Shader.h"
#include <GL/glew.h>
#include <glm/glm.hpp>


class BoxRenderer : public Component {
private:
    MeshGPUHandle m_Handle;
    float m_Width, m_Height, m_Depth;
    glm::vec3 m_Colour{};
public:
    BoxRenderer(Entity& owner, glm::vec3 colour);
    BoxRenderer(Entity& owner, float width = 1.0f, float height = 1.0f, float depth = 1.0f)
        : Component(owner), m_Width(width), m_Height(height), m_Depth(depth) { initialize(); }

    ~BoxRenderer() override { destroyMesh(m_Handle); }

    void initialize() override
    {
        MeshData mesh = generateBoxGeometry(m_Width, m_Height, m_Depth);
        m_Handle = uploadMesh(mesh);
    }

    void render() const override
    {
        Shader* shader = getComponent<Shader>();
        if (!shader) {
            renderLegacy();
            return;
        }
        shader->bind();
        drawMesh(m_Handle);
        if (shader) shader->unbind();
    }
protected:
    void renderLegacy() const;
    glm::vec2 m_Size;
};

#endif //MAPLEENGINE_BOXRENDERER_H
