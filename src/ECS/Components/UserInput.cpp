#include "UserInput.h"
#include "RigidBody.h"

/// \param owner Entity containing this component
/// \param speed Movement speed
UserInput::UserInput(Entity &owner, float speed) : Component(owner)
{
    m_Speed = speed;
}

/// Move entity with arrow keys.
/// \param window OpenGL window context.
/// \param deltaTime Scene frame time.
void UserInput::update(GLFWwindow* window, float deltaTime)
{
    float speed = m_Speed * deltaTime;
    b2Body* body = getBody();

    if (body == nullptr)
        return;

    if (glfwGetKey(window, GLFW_KEY_LEFT) || glfwGetKey(window, GLFW_KEY_A))
        body->SetTransform(b2Vec2(body->GetPosition().x - speed, body->GetPosition().y), body->GetAngle());
    if (glfwGetKey(window, GLFW_KEY_RIGHT) || glfwGetKey(window, GLFW_KEY_D))
        body->SetTransform(b2Vec2(body->GetPosition().x + speed, body->GetPosition().y), body->GetAngle());

    // Does physics apply?
    if (!getEntity().getComponment<RigidBody>())
    {
        if (glfwGetKey(window, GLFW_KEY_UP) || glfwGetKey(window, GLFW_KEY_W))
            body->SetTransform(b2Vec2(body->GetPosition().x, body->GetPosition().y + speed), body->GetAngle());
        if (glfwGetKey(window, GLFW_KEY_DOWN) || glfwGetKey(window, GLFW_KEY_S))
            body->SetTransform(b2Vec2(body->GetPosition().x, body->GetPosition().y - speed), body->GetAngle());
    }

}