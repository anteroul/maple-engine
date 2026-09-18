#ifndef MAPLEENGINE_MOUSEFOLLOW_H
#define MAPLEENGINE_MOUSEFOLLOW_H

#include <GLFW/glfw3.h>
#include <box2d/box2d.h>
#include "../ECS/Component.h"
#include "../ECS/Components/RigidBody.h"

#define SPEED (0.2)

/// Drive an entity from the mouse position.
///
/// A kinematic body is moved by giving it a velocity, not by teleporting it:
/// a body that is moved with b2Body_SetTransform skips straight past whatever
/// is between the two positions, so the solver never sees the contact. Solving
/// for the velocity that lands on the cursor this frame keeps the motion
/// continuous and lets the body push what it runs into.
static void mouseFollow(Component* owner, GLFWwindow *window, float deltaTime)
{
    double x, y;
    int width, height;

    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &x, &y);

    x = x - (width / 2);
    x /= (width / 2);

    y = y - (height / 2);
    y *= -1;
    y /= (height / 2);

    b2BodyId body = owner->getBody();

    if (B2_IS_NULL(body) || deltaTime <= 0.f)
        return;

    const b2Vec2 position = b2Body_GetPosition(body);

    // An entity with a RigidBody is simulated, so the cursor only nudges it
    // sideways and gravity keeps owning its vertical motion. Anything else
    // follows the cursor outright.
    const b2Vec2 target = owner->getEntity().getComponent<RigidBody>()
            ? b2Vec2{position.x + static_cast<float>(SPEED * x), position.y}
            : b2Vec2{static_cast<float>(x), static_cast<float>(y)};

    if (b2Body_GetType(body) == b2_staticBody)
    {
        b2Body_SetTransform(body, target, b2Body_GetRotation(body));
        return;
    }

    b2Body_SetLinearVelocity(body, b2MulSV(1.f / deltaTime, b2Sub(target, position)));
}

#endif //MAPLEENGINE_MOUSEFOLLOW_H
