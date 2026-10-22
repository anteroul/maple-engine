#ifndef MAPLEENGINE_SHOOTBYCLICKING_H
#define MAPLEENGINE_SHOOTBYCLICKING_H

// Engine headers first: they pull in GL/glew.h, which must precede glfw3.h.
#include "MouseFollow.h"
#include "ExpireAfter.h"
#include "../ECS/Components/BehaviourScript.h"
#include "../ECS/Components/RigidBody.h"
#include "../ECS/Components/SphereRenderer.h"
#include "../World.h"
#include <GLFW/glfw3.h>
#include <box2d/box2d.h>
#include <glm/glm.hpp>
#include <cmath>
#include <cstdlib>
#include <vector>

/// Fewest and most particles a single click can spawn.
constexpr int SHOOT_MIN_PARTICLES = 18;
constexpr int SHOOT_MAX_PARTICLES = 84;
/// Half the width of a particle's box, in world units.
constexpr float SHOOT_PARTICLE_HALF_SIZE = 0.01f;
/// Launch speed range, in world units per second.
constexpr float SHOOT_MIN_SPEED = 0.5f;
constexpr float SHOOT_MAX_SPEED = 1.5f;
/// Seconds a particle lives before the world removes it. Without a limit
/// every click would leave dozens of bodies behind for good.
constexpr float SHOOT_PARTICLE_LIFETIME = 3.f;

inline float randomUnit()
{
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

/// Keep every shape of a body out of collisions with other particles.
///
/// A burst starts with all of its particles stacked on one point. Left to
/// collide, the solver would spend the first steps shoving them apart, and
/// the burst would explode outwards far faster than it was launched. Shapes
/// that share a negative group index never collide with each other, but still
/// collide with everything else.
inline void excludeFromParticleCollisions(b2BodyId body)
{
    constexpr int PARTICLE_GROUP = -1;

    std::vector<b2ShapeId> shapes(static_cast<size_t>(b2Body_GetShapeCount(body)));
    const int count = b2Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));

    for (int i = 0; i < count; ++i)
    {
        b2Filter filter = b2Shape_GetFilter(shapes[static_cast<size_t>(i)]);
        filter.groupIndex = PARTICLE_GROUP;
        b2Shape_SetFilter(shapes[static_cast<size_t>(i)], filter);
    }
}

/// Fire a burst of particles from the owner's position, fanned out in random
/// directions at random speeds.
inline void spawnParticleBurst(Component* owner)
{
    World& game = World::getInstance();
    const b2Vec2 origin = b2Body_GetPosition(owner->getBody());
    const int particleCount =
            SHOOT_MIN_PARTICLES + rand() % (SHOOT_MAX_PARTICLES - SHOOT_MIN_PARTICLES + 1);

    for (int i = 0; i < particleCount; ++i)
    {
        const b2Vec2 halfSize{SHOOT_PARTICLE_HALF_SIZE, SHOOT_PARTICLE_HALF_SIZE};
        auto particle = new Entity(game.getPhysics().getWorld(),
                                   b2Vec2{origin.x - halfSize.x, origin.y + halfSize.y},
                                   b2Vec2{origin.x + halfSize.x, origin.y - halfSize.y},
                                   BodyType::Dynamic);

        excludeFromParticleCollisions(particle->body);

        const float angle = randomUnit() * 2.f * 3.14159265f;
        const float speed = SHOOT_MIN_SPEED + randomUnit() * (SHOOT_MAX_SPEED - SHOOT_MIN_SPEED);
        b2Body_SetLinearVelocity(particle->body,
                                 b2Vec2{std::cos(angle) * speed, std::sin(angle) * speed});

        auto renderer = new SphereRenderer(*particle, SHOOT_PARTICLE_HALF_SIZE, 8, 8);
        renderer->setColour(glm::vec3(randomUnit(), randomUnit(), randomUnit()));

        particle->addComponent(new RigidBody(*particle, 0.01f));
        particle->addComponent(renderer);
        particle->addComponent(new BehaviourScript(*particle, expireAfter(SHOOT_PARTICLE_LIFETIME)));

        // The world takes ownership. It is iterating its entities right now
        // (this runs from a component update), so the particle is queued and
        // joins at the end of the update.
        game.addEntity(particle);
    }
}

/// Crosshair that follows the mouse and fires a particle burst on each click.
///
/// Attach it to a kinematic entity with a BehaviourScript. The button is edge
/// detected, so holding it down fires once, not once per frame. The state is
/// captured per script, so each crosshair tracks its own button.
inline auto shootByClicking()
{
    return [wasPressed = false](Component* owner, GLFWwindow* window, float deltaTime) mutable
    {
        mouseFollow(owner, window, deltaTime);

        const bool isPressed =
                glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        if (isPressed && !wasPressed && !B2_IS_NULL(owner->getBody()))
            spawnParticleBurst(owner);

        wasPressed = isPressed;
    };
}

#endif //MAPLEENGINE_SHOOTBYCLICKING_H
