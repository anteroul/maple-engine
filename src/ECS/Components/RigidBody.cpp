#include "RigidBody.h"

void RigidBody::update(GLFWwindow *window, float deltaTime)
{
    b2Body* body = getBody();
    b2Vec2 position = body->GetPosition();
    const float angle = body->GetAngle();
    const float height = m_Owner->size.y;
    const float bottom = position.y - height;

    if (const float surface = ground->body->GetPosition().y + ground->size.y; bottom < surface && bottom < ground->body->GetPosition().y)
    {
        body->SetTransform(b2Vec2(position.x, surface), angle);
    }
}
