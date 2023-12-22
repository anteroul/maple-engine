#include "MouseFollow.h"
#include "RigidBody.h"

/// Sets the entity position as current cursor position.
/// \param window OpenGL window context.
/// \param deltaTime Scene frame time.
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

    b2Body* body = getBody();

    // Is gravity applied?
    if (!getEntity().getComponment<RigidBody>()) {
        body->SetTransform(b2Vec2((float)x, (float)y), body->GetAngle());
    } else {
        body->SetTransform(b2Vec2(body->GetPosition().x + speed * x, body->GetPosition().y), body->GetAngle());
    }
}