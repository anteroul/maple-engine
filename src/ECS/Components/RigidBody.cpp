#include "RigidBody.h"

void RigidBody::update(GLFWwindow *window, float deltaTime)
{
    b2BodyId body = getBody();
    b2Vec2 position = b2Body_GetPosition(body);
    float height = m_Owner->size.y;
    float width = m_Owner->size.x;
    float bottom = position.y - height / 2.0f;

    float highestSurface = -FLT_MAX;
    bool onSurface = false;

    for (auto &i : *m_Entities)
    {
        if (i == m_Owner)
            continue;

        b2Vec2 otherPosition = b2Body_GetPosition(i->body);
        float otherWidth = i->size.x / 2.0f;
        float leftA = position.x - width / 2.0f;
        float rightA = position.x + width / 2.0f;
        float leftB = otherPosition.x - otherWidth;
        float rightB = otherPosition.x + otherWidth;

        if (rightA >= leftB && leftA <= rightB)
        {
            float surface = otherPosition.y + i->size.y / 2.0f;

            // Landed or penetrating, not just "suspiciously close":
            // bottom at/below the surface counts, with a small
            // allowance so we don't miss it due to float error.
            const float skin = 0.01f;
            if (bottom <= surface + skin)
            {
                if (surface > highestSurface)
                {
                    highestSurface = surface;
                    onSurface = true;
                }
            }
        }
    }

    if (onSurface)
    {
        // Snap to rest on the highest surface found, and kill fall speed
        // so next freefall starts from 0, not leftover impact speed.
        b2Rot rotation = b2Body_GetRotation(body);
        b2Body_SetTransform(body, b2Vec2{position.x, highestSurface + height / 2.0f}, rotation);
        m_Speed = 0.0f;
    }

    m_OnFreefall = !onSurface;
}