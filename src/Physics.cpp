#include "Physics.h"
#include "ECS/Components/RigidBody.h"

#define GRAVITY (9.81 / 1000)

Physics::Physics() : world(b2Vec2_zero), accumulator(0.f)
{}

void Physics::update(Entity* entity, float deltaTime)
{
    b2Body* body = entity->body;
    b2Vec2 position = body->GetPosition();
    float angle = body->GetAngle();

    while (accumulator >= getStepSize())
    {
        world.Step(getStepSize(), getVelocitySolverIterations(), getPositionSolverIterations());
        accumulator -= getStepSize();
    }
    float fallingSpeed = getAcceleration(GRAVITY, deltaTime);
    if (entity->getComponment<RigidBody>())
    {
        fallingSpeed *= entity->getComponment<RigidBody>()->getMass();
    }
    body->SetTransform(b2Vec2(position.x, position.y - fallingSpeed), angle);
}

float Physics::getForce(float mass, float distance, float deltaTime)
{
    return mass * getAcceleration(distance, deltaTime);
}

float Physics::getAcceleration(float distance, float deltaTime)
{
    return distance / deltaTime * deltaTime;
}