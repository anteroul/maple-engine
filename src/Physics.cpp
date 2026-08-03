#include "Physics.h"
#include "ECS/Components/RigidBody.h"

constexpr float GRAVITY = 9.81f; // m/s^2, real units, mass-independent

Physics::Physics() : accumulator(0.f)
{
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = b2Vec2_zero; // world gravity off; this system owns freefall
    world = b2CreateWorld(&worldDef);
}

Physics::~Physics()
{
    b2DestroyWorld(world);
}

void Physics::update(Entity* entity, float deltaTime)
{
    RigidBody* rb = entity->getComponent<RigidBody>();
    if (!rb->onFreefall()) return;

    accumulator += deltaTime;
    while (accumulator >= getStepSize())
    {
        b2World_Step(world, getStepSize(), getSubStepCount());
        accumulator -= getStepSize();
    }

    b2BodyId body = entity->body;
    b2Vec2 position = b2Body_GetPosition(body);
    b2Rot rotation = b2Body_GetRotation(body);

    // v += a * dt  (acceleration due to gravity, independent of mass)
    rb->m_Speed += GRAVITY * deltaTime;

    // x += v * dt
    float fallDistance = rb->m_Speed * deltaTime;

    b2Body_SetTransform(body, b2Vec2{position.x, position.y - fallDistance}, rotation);
}