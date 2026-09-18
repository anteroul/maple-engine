#ifndef MAPLEENGINE_PHYSICS_H
#define MAPLEENGINE_PHYSICS_H

#include <box2d/box2d.h>
#include "ECS/Component.h"
#include <map>
#include <list>
#include <vector>

/// Owns the Box2D world and advances it on a fixed timestep.
///
/// The world is stepped exactly once per frame, from World::update, and it
/// owns gravity, contacts and restitution. Nothing outside this class should
/// integrate motion by hand: write forces or velocities to a body and let the
/// solver move it.
class Physics {
public:
    /// \param gravity World gravity in m/s^2. Defaults to earth gravity,
    ///                pointing down the negative y axis.
    explicit Physics(b2Vec2 gravity = b2Vec2{0.f, -9.81f});
    ~Physics();

    Physics(const Physics&) = delete;
    Physics& operator=(const Physics&) = delete;

    /// Advance the whole world by as many fixed steps as deltaTime allows.
    /// Call this once per frame, after components have written their forces
    /// and velocities and before the render transforms are synced.
    /// \param deltaTime Wall-clock frame time in seconds.
    void step(float deltaTime);

    /// \return id of game world
    b2WorldId getWorld() const { return world; }

    /// Fraction of a fixed step still unconsumed, in [0, 1). Renderers can use
    /// this to interpolate between the last two physics states so that a fixed
    /// step does not judder against a variable frame rate.
    float getAlpha() const { return static_cast<float>(accumulator) / getStepSize(); }

    static float getForce(float mass, float speed, float deltaTime);
    static float getAcceleration(float speed, float deltaTime);

    static float getStepSize() { return 1.f/60.f; }
    static int getSubStepCount() { return 4; }
private:
    /// Longest frame we are willing to simulate in one go. Without this, one
    /// long stall (a breakpoint, a window drag) queues up hundreds of steps
    /// and the next frame takes even longer, which queues up more still.
    static constexpr float MAX_FRAME_TIME = 0.25f;

    b2WorldId world;
    double accumulator;
};


#endif //MAPLEENGINE_PHYSICS_H
