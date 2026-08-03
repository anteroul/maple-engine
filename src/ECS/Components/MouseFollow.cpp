#include "MouseFollow.h"
#include "RigidBody.h"

/// Sets the entity position as current cursor position.
/// \param window OpenGL window context.
/// \param deltaTime World frame time.
void MouseFollow::update(GLFWwindow *window, float deltaTime)
{
    double x, y;
    int width, height;
    float speed = m_Speed * deltaTime;

    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &x, &y);

    x = x - (width / 2);
    x /= (width / 2);

    y = y - (height / 2);
    y *= -1;
    y /= (height / 2);

    b2BodyId body = getBody();

    // Is gravity applied?
    if (!getEntity().getComponent<RigidBody>()) {
        b2Body_SetTransform(body, b2Vec2{(float)x, (float)y}, b2Body_GetRotation(body));
    } else {
        b2Body_SetTransform(body, b2Vec2{b2Body_GetPosition(body).x + speed * (float)x, b2Body_GetPosition(body).y}, b2Body_GetRotation(body));
    }
}
