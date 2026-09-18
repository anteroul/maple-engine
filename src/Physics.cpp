#include "Physics.h"
#include "ECS/Components/RigidBody.h"

Physics::Physics(b2Vec2 gravity) : accumulator(0.0)
{
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = gravity;
    world = b2CreateWorld(&worldDef);
}

Physics::~Physics()
{
    b2DestroyWorld(world);
}

/// Advance the world on a fixed timestep.
/// \param deltaTime World frame time in seconds.
void Physics::step(float deltaTime)
{
    if (deltaTime <= 0.f) return;
    if (deltaTime > MAX_FRAME_TIME) deltaTime = MAX_FRAME_TIME;

    accumulator += deltaTime;

    while (accumulator >= getStepSize())
    {
        b2World_Step(world, getStepSize(), getSubStepCount());
        accumulator -= getStepSize();
    }
}

/// Newton's second law: F = m * a.
/// \param mass Mass of the body in kg.
/// \param speed Change in speed over deltaTime, in m/s.
/// \param deltaTime World frame time in seconds.
/// \return Force in newtons.
float Physics::getForce(float mass, float speed, float deltaTime)
{
    return mass * getAcceleration(speed, deltaTime);
}

/// Average acceleration over a frame: a = dv / dt.
/// \param speed Change in speed over deltaTime, in m/s.
/// \param deltaTime World frame time in seconds.
/// \return Acceleration in m/s^2, or 0 for a zero-length frame.
float Physics::getAcceleration(float speed, float deltaTime)
{
    if (deltaTime <= 0.f) return 0.f;
    return speed / deltaTime;
}
