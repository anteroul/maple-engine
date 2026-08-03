#include "UserInput.h"
#include "RigidBody.h"
#include "../../Physics.h"

/// \param owner Entity containing this component
/// \param speed Movement speed
UserInput::UserInput(Entity &owner, float speed) : Component(owner)
{
    m_Speed = speed;
}

/// Move entity with arrow keys.
/// \param window OpenGL window context.
/// \param deltaTime World frame time.
void UserInput::update(GLFWwindow* window, float deltaTime)
{
    b2BodyId body = getBody();

    if (B2_IS_NULL(body))
        return;

    // Do physics apply?
    if (getEntity().getComponent<RigidBody>())
    {
        float acceleration = Physics::getAcceleration(m_Speed, deltaTime) * deltaTime / getEntity().getComponent<RigidBody>()->getMass();

        if (glfwGetKey(window, GLFW_KEY_LEFT) || glfwGetKey(window, GLFW_KEY_A))
            b2Body_SetTransform(body, b2Vec2{b2Body_GetPosition(body).x - acceleration, b2Body_GetPosition(body).y}, b2Body_GetRotation(body));
        if (glfwGetKey(window, GLFW_KEY_RIGHT) || glfwGetKey(window, GLFW_KEY_D))
            b2Body_SetTransform(body, b2Vec2{b2Body_GetPosition(body).x + acceleration, b2Body_GetPosition(body).y}, b2Body_GetRotation(body));
    } else {
        if (glfwGetKey(window, GLFW_KEY_LEFT) || glfwGetKey(window, GLFW_KEY_A))
            b2Body_SetTransform(body, b2Vec2{b2Body_GetPosition(body).x - m_Speed, b2Body_GetPosition(body).y}, b2Body_GetRotation(body));
        if (glfwGetKey(window, GLFW_KEY_RIGHT) || glfwGetKey(window, GLFW_KEY_D))
            b2Body_SetTransform(body, b2Vec2{b2Body_GetPosition(body).x + m_Speed, b2Body_GetPosition(body).y}, b2Body_GetRotation(body));
        if (glfwGetKey(window, GLFW_KEY_UP) || glfwGetKey(window, GLFW_KEY_W))
            b2Body_SetTransform(body, b2Vec2{b2Body_GetPosition(body).x, b2Body_GetPosition(body).y + m_Speed}, b2Body_GetRotation(body));
        if (glfwGetKey(window, GLFW_KEY_DOWN) || glfwGetKey(window, GLFW_KEY_S))
            b2Body_SetTransform(body, b2Vec2{b2Body_GetPosition(body).x, b2Body_GetPosition(body).y - m_Speed}, b2Body_GetRotation(body));
    }
}
