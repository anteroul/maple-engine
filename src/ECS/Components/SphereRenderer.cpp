#include "SphereRenderer.h"

#define SLICES  32

/// \param owner The entity containing this component
/// \param radius Sphere radius
/// \param colour Sphere colour
SphereRenderer::SphereRenderer(Entity& owner, glm::vec3 colour) : Component(owner)
{
    m_Radius = (owner.size.x + owner.size.y) / 2;
    m_Quadric = gluNewQuadric();
    m_Colour = colour;
}

/// Render sphere
void SphereRenderer::renderLegacy() const
{
    const b2BodyId body = getBody();

    glLoadIdentity();
    b2Vec2 ballPosition = b2Body_GetPosition(body);
    glTranslatef(ballPosition.x, ballPosition.y, 0.f);
    glColor3f(m_Colour.r, m_Colour.g, m_Colour.b);
    gluSphere(m_Quadric, m_Radius, SLICES, SLICES);
    glFlush();
}