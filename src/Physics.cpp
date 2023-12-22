#include "Physics.h"

#define GRAVITY (9.81 / 10)

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
        world.Step(getStepSize(), getVelocitySolverIterations(), getPositionSolverIterations());
    }
    accumulator += deltaTime;
    float fallingSpeed = GRAVITY * deltaTime;
    body->SetTransform(b2Vec2(position.x, position.y - fallingSpeed), angle);
}

float Physics::getGravity(float deltaTime)
{
    return GRAVITY * deltaTime;
}
