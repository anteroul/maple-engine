#include <box2d/box2d.h>
#include <box2d/id.h>
#include <box2d/collision.h>
#include <box2d/math_functions.h>
#include <box2d/base.h>
#include <box2d/types.h>
#include "../ECS/Components/BehaviourScript.h"
#include "../ECS/Components/RigidBody.h"
#include "../ECS/Components/SphereRenderer.h"
#include "../ECS/Components/Shader.h"
#include <cstdlib>
#include <cmath>
#include <glm/glm.hpp>
#include "../World.h"

void update(Component* component, GLFWwindow* window, float deltaTime)
{
    static bool m_WasPressed = false;

    moveCrosshair(component, window);

    bool isPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // Fire only on the frame the button transitions up -> down,
    // not every frame it's held.
    if (isPressed && !m_WasPressed)
    {
        spawnParticleBurst(component, window, deltaTime);
    }

    m_WasPressed = isPressed;
}

void moveCrosshair(Component* component, GLFWwindow* window)
{
    double x, y;
    int width, height;

    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &x, &y);

    x = (x - width / 2.0) / (width / 2.0);
    y = -(y - height / 2.0) / (height / 2.0);

    b2BodyId body = component->getBody();
    b2Body_SetTransform(body, b2Vec2{(float)x, (float)y}, b2Body_GetRotation(body));
}

void spawnParticleBurst(Component* component, GLFWwindow* window, float deltaTime)
{
    b2Vec2 origin = b2Body_GetPosition(component->getBody());
    b2WorldId world = b2Body_GetWorld(component->getBody());
    int m_ParticlesPerClick = rand() % 67 + 18;

    for (int i = 0; i < m_ParticlesPerClick; ++i)
    {
        // New entity per particle. Adjust to however your engine
        // actually registers/tracks entities (factory, scene list, etc.)
        Entity* particle = new Entity(component->getEntity());
        b2Vec2(particle->size.x + particle->size.y) = b2Vec2(0.1f, 0.1f);

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = origin;
        b2BodyId particleBody = b2CreateBody(world, &bodyDef);

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 1.0f;
        shapeDef.material.friction = 0.3f;
        b2Circle circle{ {0.0f, 0.0f}, 0.05f };
        b2CreateCircleShape(particleBody, &shapeDef, &circle);

        particle->body = particleBody;

        // Random direction + speed, so the burst fans out rather
        // than all particles flying in one line.
        float angle = ((float)rand() / RAND_MAX) * 2.0f * 3.14159265f;
        float speed = 2.0f + ((float)rand() / RAND_MAX) * 3.0f;
        b2Vec2 velocity{ cosf(angle) * speed, sinf(angle) * speed };
        b2Body_SetLinearVelocity(particleBody, velocity);

        // Random colour per particle
        glm::vec3 colour(
            (float)rand() / RAND_MAX,
            (float)rand() / RAND_MAX,
            (float)rand() / RAND_MAX
        );

        particle->addComponent(new Shader(*particle, "../Shaders/basic.glsl"));
        particle->addComponent(new Shader(*particle, "../Shaders/test.glsl"));

        // Register `particle` with whatever owns your entity list
        // (SceneManager's current Level, presumably) so it gets
        // updated/rendered/cleaned up like everything else.
    }
}