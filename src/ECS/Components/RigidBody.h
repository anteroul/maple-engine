#ifndef MAPLEENGINE_RIGIDBODY_H
#define MAPLEENGINE_RIGIDBODY_H

#include "../Component.h"

/// Gives an entity a mass and a handle on its simulated body.
///
/// This component does not simulate anything itself. Gravity, collision and
/// restitution all come from the Box2D world, which World steps once per
/// frame. What lives here is the gameplay-facing surface: read the velocity,
/// ask whether the entity is standing on something, push it around.
class RigidBody : public Component {
public:
    /// \param owner The entity containing this component
    /// \param mass Mass of the body in kg. Overrides the mass Box2D derives
    ///             from the shape's density. Ignored for non-dynamic bodies.
    RigidBody(Entity& owner, float mass);

    void update(GLFWwindow* window, float deltaTime) override;

    float getMass() const;
    b2Vec2 getVelocity() const;
    void setVelocity(b2Vec2 velocity);

    /// Continuous push, in newtons. Use over a span of time.
    void applyForce(b2Vec2 force);
    /// Instantaneous change of momentum, in newton-seconds. Use for a hit.
    void applyImpulse(b2Vec2 impulse);

    /// True while a contact underneath this body is supporting it. Sampled
    /// from the last world step, so scripts see the state the solver ended on.
    bool isGrounded() const { return m_Grounded; }
    bool onFreefall() const { return !m_Grounded; }
private:
    /// How upward a contact normal has to point before it counts as ground.
    /// cos(45 degrees), so anything up to a 45 degree slope is standable.
    static constexpr float GROUND_NORMAL_THRESHOLD = 0.7071f;

    void refreshGrounded();

    float m_Mass;
    bool m_Grounded;
};


#endif //MAPLEENGINE_RIGIDBODY_H
