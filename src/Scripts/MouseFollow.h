#ifndef MAPLEENGINE_MOUSEFOLLOW_H
#define MAPLEENGINE_MOUSEFOLLOW_H

#include <GLFW/glfw3.h>
#include <box2d/box2d.h>
#include "../Component.h"
#include <RigidBody.h>

#define SPEED (0.2)

static void mouseFollow(Component* owner, GLFWwindow *window, float deltaTime)
{
    double x, y;
    int width, height;
    float speed = SPEED * deltaTime;

    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &x, &y);

    x = x - (width / 2);
    x /= (width / 2);

    y = y - (height / 2);
    y *= -1;
    y /= (height / 2);

    b2BodyId body = owner->getBody();

    // Is gravity applied?
    if (!owner->getEntity().getComponent<RigidBody>()) {
        b2Body_SetTransform(body, b2Vec2{(float)x, (float)y}, b2Body_GetRotation(body));
    } else {
        b2Body_SetTransform(body, b2Vec2{b2Body_GetPosition(body).x + speed * (float)x, b2Body_GetPosition(body).y}, b2Body_GetRotation(body));
    }
}

#endif //MAPLEENGINE_MOUSEFOLLOW_H