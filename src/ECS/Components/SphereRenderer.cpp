#include "SphereRenderer.h"

#define SLICES  32


/// \param owner The entity containing this component
/// \param radius Sphere radius
/// \param colour Sphere colour
/*
SphereRenderer::SphereRenderer(Entity& owner, glm::vec3 colour) : Component(owner)
{
    m_Radius = (owner.size.x + owner.size.y) / 2;
    m_Quadric = gluNewQuadric();
    m_Colour = colour;
}
*/

/// Render sphere
void SphereRenderer::renderLegacy() const
{
    // Read the transform the last physics step published rather than the body
    // directly, so that everything drawn this frame comes from one snapshot.
    const Transform& transform = getEntity().transform;

    glLoadIdentity();
    glTranslatef(transform.x, transform.y, 0.f);
    glRotatef(transform.rotation * 180.f / 3.14159265f, 0.f, 0.f, 1.f);
    glColor3f(m_Colour.r, m_Colour.g, m_Colour.b);
    gluSphere(m_Quadric, m_Radius, SLICES, SLICES);
    glFlush();
}