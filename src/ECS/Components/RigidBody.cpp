#include "RigidBody.h"
#include "../../Physics.h"

void RigidBody::update(GLFWwindow *window, float deltaTime)
{
    b2Body* body = getBody();
    b2Vec2 position = body->GetPosition();
    float angle = body->GetAngle();
    float height = m_Owner->size.y;
    float bottom = position.y - height;
    float platformLength = ground->size.x / 2;
    float surface = ground->body->GetPosition().y;

    if (position.x + m_Owner->size.x < ground->body->GetPosition().x + platformLength && position.x > ground->body->GetPosition().x - platformLength)
    {
        if (bottom < surface && position.y > surface - ground->size.y)
        {
            const float impact = Physics::getGravity(deltaTime) * 2;
            body->SetTransform(b2Vec2(position.x, position.y + impact), angle);
        }
    }
}
