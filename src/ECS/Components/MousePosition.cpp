#include "MousePosition.h"

/// \param owner Entity containing this component
MousePosition::MousePosition(Entity &owner) : Component(owner)
{}

/// Sets the entity position as current cursor position.
/// \param window OpenGL window context.
/// \param deltaTime Scene frame time.
void MousePosition::update(GLFWwindow *window, float deltaTime)
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

    b2Body* body = getBody();
    body->SetTransform(b2Vec2((float)x, (float)y), body->GetAngle());
}