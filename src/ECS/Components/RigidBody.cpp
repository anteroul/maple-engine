#include "RigidBody.h"
#include "../../Physics.h"

void RigidBody::update(GLFWwindow *window, float deltaTime)
{
    b2Body* body = getBody();
    b2Vec2 position = body->GetPosition();
    float angle = body->GetAngle();
    float height = m_Owner->size.y;
    float bottom = position.y - height;
    float width = m_Owner->size.x;

    for (auto i : *m_Entities)
    {
        float otherWidth = i->size.x / 2;
        float surface = i->body->GetPosition().y;

        if (position.x + width < i->body->GetPosition().x + otherWidth &&
            position.x > i->body->GetPosition().x - otherWidth)
        {
            if (bottom < surface && position.y > surface - i->size.y)
            {
                const float impact = Physics::getForce(m_Mass, position.y, deltaTime) * -1;
                body->SetTransform(b2Vec2(position.x, Physics::getAcceleration(position.y + impact / 10, deltaTime)), angle);
            }
        }
    }
}
