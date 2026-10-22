#ifndef MAPLEENGINE_EXPIREAFTER_H
#define MAPLEENGINE_EXPIREAFTER_H

#include "../ECS/Component.h"
#include "../World.h"

/// BehaviourScript that removes the entity it is attached to once it has
/// lived for `lifetime` seconds. Use it for short-lived effects (particles,
/// debris) so they do not pile up for good.
inline auto expireAfter(float lifetime)
{
    return [lifetime, age = 0.f](Component* owner, GLFWwindow*, float deltaTime) mutable
    {
        age += deltaTime;

        if (age >= lifetime)
            World::getInstance().destroyEntity(&owner->getEntity());
    };
}

#endif //MAPLEENGINE_EXPIREAFTER_H
