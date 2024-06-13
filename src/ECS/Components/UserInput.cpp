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
    b2Body* body = getBody();

    if (body == nullptr)
        return;

    // Do physics apply?
    if (getEntity().getComponent<RigidBody>())
    {
        float acceleration = Physics::getAcceleration(m_Speed, deltaTime) * deltaTime / getEntity().getComponent<RigidBody>()->getMass();

        if (glfwGetKey(window, GLFW_KEY_LEFT) || glfwGetKey(window, GLFW_KEY_A))
            body->SetTransform(b2Vec2(body->GetPosition().x - acceleration, body->GetPosition().y), body->GetAngle());
        if (glfwGetKey(window, GLFW_KEY_RIGHT) || glfwGetKey(window, GLFW_KEY_D))
            body->SetTransform(b2Vec2(body->GetPosition().x + acceleration, body->GetPosition().y), body->GetAngle());
    } else {
        if (glfwGetKey(window, GLFW_KEY_LEFT) || glfwGetKey(window, GLFW_KEY_A))
            body->SetTransform(b2Vec2(body->GetPosition().x - m_Speed, body->GetPosition().y), body->GetAngle());
        if (glfwGetKey(window, GLFW_KEY_RIGHT) || glfwGetKey(window, GLFW_KEY_D))
            body->SetTransform(b2Vec2(body->GetPosition().x + m_Speed, body->GetPosition().y), body->GetAngle());
        if (glfwGetKey(window, GLFW_KEY_UP) || glfwGetKey(window, GLFW_KEY_W))
            body->SetTransform(b2Vec2(body->GetPosition().x, body->GetPosition().y + m_Speed), body->GetAngle());
        if (glfwGetKey(window, GLFW_KEY_DOWN) || glfwGetKey(window, GLFW_KEY_S))
            body->SetTransform(b2Vec2(body->GetPosition().x, body->GetPosition().y - m_Speed), body->GetAngle());
    }
}