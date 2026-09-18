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
///
/// A simulated entity is pushed, not placed: applying a force leaves the
/// solver in charge of the resulting motion, so the entity still collides,
/// still slides along the ground and still falls. Entities without a
/// RigidBody have no simulation to respect and are moved directly.
/// \param window OpenGL window context.
/// \param deltaTime World frame time.
void UserInput::update(GLFWwindow* window, float deltaTime)
{
    b2BodyId body = getBody();

    if (B2_IS_NULL(body))
        return;

    const bool left = glfwGetKey(window, GLFW_KEY_LEFT) || glfwGetKey(window, GLFW_KEY_A);
    const bool right = glfwGetKey(window, GLFW_KEY_RIGHT) || glfwGetKey(window, GLFW_KEY_D);
    const bool up = glfwGetKey(window, GLFW_KEY_UP) || glfwGetKey(window, GLFW_KEY_W);
    const bool down = glfwGetKey(window, GLFW_KEY_DOWN) || glfwGetKey(window, GLFW_KEY_S);

    RigidBody* rigidBody = getEntity().getComponent<RigidBody>();

    // Do physics apply?
    if (rigidBody)
    {
        const float force = Physics::getForce(rigidBody->getMass(), m_Speed, deltaTime);

        if (left)
            rigidBody->applyForce(b2Vec2{-force, 0.f});
        if (right)
            rigidBody->applyForce(b2Vec2{force, 0.f});
    } else {
        b2Vec2 position = b2Body_GetPosition(body);

        if (left)
            position.x -= m_Speed;
        if (right)
            position.x += m_Speed;
        if (up)
            position.y += m_Speed;
        if (down)
            position.y -= m_Speed;

        b2Body_SetTransform(body, position, b2Body_GetRotation(body));
    }
}
