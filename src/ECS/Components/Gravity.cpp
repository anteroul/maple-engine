#include "Gravity.h"

#define GRAVITY (9.81 / 10000)

void Gravity::update(GLFWwindow *window, float deltaTime)
{
    b2Body* body = getBody();
    float timeStep = 1.0f / 60.0f;
    int32 velocityIterations = 6;
    int32 positionIterations = 2;

    for (int32 i = 0; i < 60; ++i)
    {
        m_World->Step(timeStep, velocityIterations, positionIterations);
        b2Vec2 position = body->GetPosition();
        float angle = body->GetAngle();
        float height = m_Owner->size.y;
        float bottom = position.y - height;

        if (bottom < ground->body->GetPosition().y)
            body->SetTransform(b2Vec2(position.x, position.y + GRAVITY), angle);
        else
            body->SetTransform(b2Vec2(position.x, position.y - GRAVITY), angle);
    }
}
