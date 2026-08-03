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
    const b2BodyId body = getBody();
    const b2Vec2 position = b2Body_GetPosition(body);
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