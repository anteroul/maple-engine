#ifndef MAPLEENGINE_MOUSEFOLLOW_H
#define MAPLEENGINE_MOUSEFOLLOW_H

// Engine headers first: they pull in GL/glew.h, which must precede glfw3.h.
#include "../ECS/Component.h"
#include "../ECS/Components/RigidBody.h"
#include <GLFW/glfw3.h>
#include <box2d/box2d.h>

/// How far, per frame, the cursor nudges an entity that has a RigidBody, as a
/// fraction of the cursor's distance from the centre of the window.
constexpr float MOUSE_FOLLOW_SPEED = 0.2f;

/// Cursor position in normalised device coordinates: (-1, -1) is the bottom
/// left of the window, (1, 1) the top right.
/// \return false while the window has no area (minimised), when there is no
///         meaningful position to report.
inline bool getCursorNDC(GLFWwindow* window, b2Vec2& cursor)
{
    double x, y;
    int width, height;

    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &x, &y);

    if (width <= 0 || height <= 0)
        return false;

    const double halfWidth = width / 2.0;
    const double halfHeight = height / 2.0;

    cursor.x = static_cast<float>((x - halfWidth) / halfWidth);
    cursor.y = static_cast<float>(-(y - halfHeight) / halfHeight);
    return true;
}

/// Drive an entity from the mouse position.
///
/// A kinematic body is moved by giving it a velocity, not by teleporting it:
/// a body that is moved with b2Body_SetTransform skips straight past whatever
/// is between the two positions, so the solver never sees the contact. Solving
/// for the velocity that lands on the cursor this frame keeps the motion
/// continuous and lets the body push what it runs into.
inline void mouseFollow(Component* owner, GLFWwindow *window, float deltaTime)
{
    b2BodyId body = owner->getBody();
    b2Vec2 cursor;

    if (B2_IS_NULL(body) || deltaTime <= 0.f || !getCursorNDC(window, cursor))
        return;

    const b2Vec2 position = b2Body_GetPosition(body);

    if (b2Body_GetType(body) == b2_staticBody)
    {
        b2Body_SetTransform(body, cursor, b2Body_GetRotation(body));
        return;
    }

    // An entity with a RigidBody is simulated, so the cursor only nudges it
    // sideways. Its vertical velocity is left alone: overwriting it every
    // frame would cancel whatever gravity added during the last step.
    if (owner->getEntity().getComponent<RigidBody>())
    {
        const b2Vec2 velocity = b2Body_GetLinearVelocity(body);
        const float nudge = MOUSE_FOLLOW_SPEED * cursor.x;
        b2Body_SetLinearVelocity(body, b2Vec2{nudge / deltaTime, velocity.y});
        return;
    }

    // Anything else follows the cursor outright.
    b2Body_SetLinearVelocity(body, b2MulSV(1.f / deltaTime, b2Sub(cursor, position)));
}

#endif //MAPLEENGINE_MOUSEFOLLOW_H
