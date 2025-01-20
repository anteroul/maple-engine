#include "Physics.h"
#include "ECS/Components/RigidBody.h"

#define GRAVITY (9.81 / 10000)

Physics::Physics() : world(b2Vec2_zero), accumulator(0.f)
{}

void Physics::update(Entity* entity, float deltaTime)
{
    if (!entity->getComponent<RigidBody>()->onFreefall()) return;
    b2Body* body = entity->body;
    b2Vec2 position = body->GetPosition();
    float angle = body->GetAngle();
    float fallingSpeed = entity->getComponent<RigidBody>()->m_Speed;

    while (accumulator >= getStepSize())
    {
        world.Step(getStepSize(), getVelocitySolverIterations(), getPositionSolverIterations());
        accumulator -= getStepSize();
    }
    entity->getComponent<RigidBody>()->m_Speed += getForce(entity->getComponent<RigidBody>()->getMass(), GRAVITY, deltaTime) * deltaTime;
    fallingSpeed = getAcceleration(entity->getComponent<RigidBody>()->m_Speed, deltaTime) * deltaTime;
    body->SetTransform(b2Vec2(position.x, position.y - fallingSpeed), angle);
}

float Physics::getForce(float mass, float speed, float deltaTime)
{
    return mass * getAcceleration(speed, deltaTime);
}

float Physics::getAcceleration(float speed, float deltaTime)
{
    return speed / deltaTime;
}