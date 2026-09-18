#include "RigidBody.h"
#include <vector>

RigidBody::RigidBody(Entity& owner, float mass)
    : Component(owner), m_Mass(mass), m_Grounded(false)
{
    b2BodyId body = getBody();

    if (B2_IS_NULL(body) || b2Body_GetType(body) != b2_dynamicBody)
        return;

    // Box2D derives mass from the shape's density and area. Scale that result
    // to the mass the caller asked for, keeping the centre of mass and the
    // rotational inertia consistent with the shape.
    b2MassData massData = b2Body_GetMassData(body);

    if (massData.mass > 0.f && m_Mass > 0.f)
    {
        const float scale = m_Mass / massData.mass;
        massData.mass = m_Mass;
        massData.rotationalInertia *= scale;
        b2Body_SetMassData(body, massData);
    }
}

/// Sample the contacts the last world step produced.
/// \param window OpenGL window context, unused.
/// \param deltaTime World frame time, unused.
void RigidBody::update(GLFWwindow* window, float deltaTime)
{
    refreshGrounded();
}

float RigidBody::getMass() const
{
    b2BodyId body = getBody();

    if (B2_IS_NULL(body))
        return m_Mass;

    return b2Body_GetMass(body);
}

b2Vec2 RigidBody::getVelocity() const
{
    b2BodyId body = getBody();

    if (B2_IS_NULL(body))
        return b2Vec2_zero;

    return b2Body_GetLinearVelocity(body);
}

void RigidBody::setVelocity(b2Vec2 velocity)
{
    b2BodyId body = getBody();

    if (!B2_IS_NULL(body))
        b2Body_SetLinearVelocity(body, velocity);
}

void RigidBody::applyForce(b2Vec2 force)
{
    b2BodyId body = getBody();

    if (!B2_IS_NULL(body))
        b2Body_ApplyForceToCenter(body, force, true);
}

void RigidBody::applyImpulse(b2Vec2 impulse)
{
    b2BodyId body = getBody();

    if (!B2_IS_NULL(body))
        b2Body_ApplyLinearImpulseToCenter(body, impulse, true);
}

/// Walk this body's touching contacts and look for one holding it up.
///
/// This replaces the old axis-aligned scan over every other entity: it costs
/// one pass over the contacts the broad phase already found, it sees rotated
/// and non-box shapes, and it agrees with whatever the solver actually did.
void RigidBody::refreshGrounded()
{
    m_Grounded = false;

    b2BodyId body = getBody();

    if (B2_IS_NULL(body))
        return;

    const int capacity = b2Body_GetContactCapacity(body);

    if (capacity <= 0)
        return;

    std::vector<b2ContactData> contacts(static_cast<size_t>(capacity));
    const int count = b2Body_GetContactData(body, contacts.data(), capacity);

    for (int i = 0; i < count; ++i)
    {
        const b2ContactData& contact = contacts[static_cast<size_t>(i)];

        if (contact.manifold.pointCount == 0)
            continue;

        // The manifold normal points from shape A to shape B. Flip it when we
        // are A, so that it always points from whatever we are touching
        // towards us. A normal pointing up then means we are resting on it.
        const bool weAreShapeA =
            B2_ID_EQUALS(b2Shape_GetBody(contact.shapeIdA), body);
        const float normalY = weAreShapeA ? -contact.manifold.normal.y
                                          : contact.manifold.normal.y;

        if (normalY > GROUND_NORMAL_THRESHOLD)
        {
            m_Grounded = true;
            return;
        }
    }
}
