#include "RigidBody.h"

void RigidBody::update(GLFWwindow *window, float deltaTime)
{
    b2Body* body = getBody();
    b2Vec2 position = body->GetPosition();
    float height = m_Owner->size.y;
    float bottom = position.y - height / 2.0f;
    float width = m_Owner->size.x;

    float highestSurface = -FLT_MAX; // Store the highest surface detected
    bool onSurface = false;

    for (auto& i : *m_Entities)
    {
        if (i == m_Owner) continue;

        float otherWidth = i->size.x / 2.0f;
        float leftA = position.x - width / 2.0f;
        float rightA = position.x + width / 2.0f;
        float leftB = i->body->GetPosition().x - otherWidth;
        float rightB = i->body->GetPosition().x + otherWidth;

        // Check if the objects horizontally overlap
        if (rightA >= leftB && leftA <= rightB)
        {
            float surface = i->body->GetPosition().y + i->size.y / 2.0f;

            // Ensure correct vertical positioning
            if (fabs(bottom - surface) <= 0.01f)
            {
                if (surface > highestSurface) {
                    highestSurface = surface;
                    onSurface = true;
                }
            }
        }
    }

    m_OnFreefall = !onSurface;
}
