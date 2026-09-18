#include "BoxRenderer.h"

/// \param owner The entity containing this component
/// \param size Rectangle size
/// \param colour Rectangle colour
/*
BoxRenderer::BoxRenderer(Entity &owner, glm::vec3 colour) : Component(owner), m_Colour(colour)
{
    m_Width = owner.size.x;
    m_Height = owner.size.y;
    m_Size = glm::vec2(m_Width, m_Height);
}
*/

/// Render rectangle
void BoxRenderer::renderLegacy() const
{
    // Read the transform the last physics step published rather than the body
    // directly, so that everything drawn this frame comes from one snapshot.
    const Transform& transform = getEntity().transform;
    const glm::vec2 position = glm::vec2(transform.x, transform.y);
    glm::vec2 m_Size = glm::vec2(m_Width, m_Height);

    glLoadIdentity();
    glBegin(GL_QUADS);
    glTranslatef(position.x, position.y, 0.f);
    glColor3f(m_Colour.r, m_Colour.g, m_Colour.b);
    glVertex2f(position.x - m_Size.x / 2, position.y - m_Size.y);
    glVertex2f(position.x - m_Size.x / 2, position.y);
    glVertex2f(position.x + m_Size.x / 2, position.y);
    glVertex2f(position.x + m_Size.x / 2, position.y - m_Size.y);
    glEnd();
    glFlush();
}