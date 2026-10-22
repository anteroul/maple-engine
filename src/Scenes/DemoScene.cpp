#include "DemoScene.h"
#include "../ECS/Components/BoxRenderer.h"
#include "../ECS/Components/SphereRenderer.h"
#include "../ECS/Components/RigidBody.h"
#include "../ECS/Components/BehaviourScript.h"
#include "../Scripts/MouseFollow.h"

/// Spawn one object at the cursor on the frame the left button goes down.
///
/// This used to fire on every frame the button was held, which with real
/// dynamic bodies means dozens of overlapping boxes a second, all of them
/// fighting to separate.
static auto spawnOnClick()
{
    return [wasPressed = false](Component* owner, GLFWwindow* window, float) mutable
    {
        const bool isPressed =
                glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        if (isPressed && !wasPressed) {
            World& game = World::getInstance();
            const b2Vec2 cursorPosition = b2Body_GetPosition(owner->getBody());
            auto gameObject = new Entity
                    (
                            game.getPhysics().getWorld(),
                            b2Vec2{cursorPosition.x - 0.2f, cursorPosition.y + 0.2f},
                            b2Vec2{cursorPosition.x + 0.2f, cursorPosition.y - 0.2f},
                            BodyType::Dynamic
                    );

            gameObject->addComponent(new RigidBody(*gameObject, 1.5f));
            gameObject->addComponent(new SphereRenderer(*gameObject, 0.2f, 32, 16));
            game.addEntity(gameObject);
        }

        wasPressed = isPressed;
    };
}

void createDemoScene(World& game) {
    b2WorldId world = game.getPhysics().getWorld();

    // The crosshair is driven straight from the mouse, so it is kinematic
    // rather than dynamic. Its shape is a sensor: it should mark where the
    // cursor is without shoving the objects it is about to spawn there.
    PhysicsMaterial cursorMaterial;
    cursorMaterial.isSensor = true;

    auto cursor = new Entity(world, b2Vec2{-0.01f, -0.01f}, b2Vec2{0.01f, 0.01f},
                             BodyType::Kinematic, cursorMaterial);
    cursor->setName("cursor");
    cursor->addComponent(new BehaviourScript(*cursor, mouseFollow));
    cursor->addComponent(new BehaviourScript(*cursor, spawnOnClick()));
    cursor->addComponent(new BoxRenderer(*cursor, 0.04f, 0.04f, 1.0f));

    auto ground = new Entity(world, b2Vec2{-2.f, -0.4f}, b2Vec2{2.f, -0.8f},
                             BodyType::Static);
    ground->setName("ground");
    ground->addComponent(new BoxRenderer(*ground, 4.0f, 0.4f, 1.f));

    game.addEntity(ground);
    game.addEntity(cursor);
}
