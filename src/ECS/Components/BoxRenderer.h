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
    glm::vec3 m_Colour{1.f, 1.f, 1.f};
    bool m_Visible = true;
public:
    BoxRenderer(Entity& owner, float width = 1.0f, float height = 1.0f, float depth = 1.0f)
        : Component(owner), m_Width(width), m_Height(height), m_Depth(depth) { initialize(); }

    ~BoxRenderer() override { destroyMesh(m_Handle); }

    /// Runs from the constructor and again from Entity::initialize(), so it
    /// must not leak the previous mesh.
    void initialize() override
    {
        destroyMesh(m_Handle);
        MeshData mesh = generateBoxGeometry(m_Width, m_Height, m_Depth);
        m_Handle = uploadMesh(mesh);
    }

    void setColour(const glm::vec3& colour) { m_Colour = colour; }
    const glm::vec3& getColour() const { return m_Colour; }
    void setVisible(bool visible) { m_Visible = visible; }
    bool isVisible() const { return m_Visible; }

    void render() const override
    {
        if (!m_Visible)
            return;

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
};

#endif //MAPLEENGINE_BOXRENDERER_H
