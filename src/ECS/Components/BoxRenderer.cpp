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

/// Render rectangle, centred on the entity and turned with its body.
void BoxRenderer::renderLegacy() const
{
    // Read the transform the last physics step published rather than the body
    // directly, so that everything drawn this frame comes from one snapshot.
    const Transform& transform = getEntity().transform;
    const float halfWidth = m_Width / 2.f;
    const float halfHeight = m_Height / 2.f;

    glLoadIdentity();
    glTranslatef(transform.x, transform.y, 0.f);
    glRotatef(transform.rotation * 180.f / 3.14159265f, 0.f, 0.f, 1.f);
    glColor3f(m_Colour.r, m_Colour.g, m_Colour.b);
    glBegin(GL_QUADS);
    glVertex2f(-halfWidth, -halfHeight);
    glVertex2f(-halfWidth, halfHeight);
    glVertex2f(halfWidth, halfHeight);
    glVertex2f(halfWidth, -halfHeight);
    glEnd();
}
