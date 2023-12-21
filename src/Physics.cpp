#include "Physics.h"

Physics::Physics() : world(b2Vec2_zero), accumulator(0.f)
{}

void Physics::update(float deltaTime)
{
    accumulator += deltaTime;

    while (accumulator >= getStepSize())
    {
        accumulator -= getStepSize();
        world.Step(getStepSize(), getVelocitySolverIterations(), getPositionSolverIterations());
    }
}
