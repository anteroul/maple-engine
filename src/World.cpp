#include "World.h"
#include "ECS/Components/UserInput.h"
#include "ECS/Components/BoxRenderer.h"
#include "ECS/Components/SphereRenderer.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/BehaviourScript.h"
#include "Scripts/MouseFollow.h"

World World::gameInstance;

World &World::getInstance() {
    return gameInstance;
}

World::World() = default;

/// Initialize all entities
void World::initialize() {
    b2WorldId world = physics.getWorld();

    // The crosshair is driven straight from the mouse, so it is kinematic
    // rather than dynamic. Its shape is a sensor: it should mark where the
    // cursor is without shoving the objects it is about to spawn there.
    PhysicsMaterial cursorMaterial;
    cursorMaterial.isSensor = true;

    auto cursor = new Entity(world, b2Vec2{-0.01f, -0.01f}, b2Vec2{0.01f, 0.01f},
                             BodyType::Kinematic, cursorMaterial);
    cursor->setName("cursor");
    cursor->addComponent(new BehaviourScript(*cursor, mouseFollow));
    cursor->addComponent(new BoxRenderer(*cursor, 0.2f, 0.15f, 1.0f));

    auto ground = new Entity(world, b2Vec2{-2.f, -0.4f}, b2Vec2{2.f, -0.8f},
                             BodyType::Static);
    ground->setName("ground");
    ground->addComponent(new BoxRenderer(*ground, 4.0f, 1.f, 1.f));

    entities.push_back(ground);
    entities.push_back(cursor);

    for (auto i: entities)
        i->initialize();
}

/// Spawn one object at the cursor on the frame the left button goes down.
///
/// This used to fire on every frame the button was held, which with real
/// dynamic bodies means dozens of overlapping boxes a second, all of them
/// fighting to separate.
void World::handleSpawnInput(GLFWwindow *window) {
    const bool isPressed =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (isPressed && !spawnButtonWasPressed) {
        auto cursor = getEntityWithName("cursor");

        if (cursor != nullptr) {
            b2Vec2 cursorPosition = b2Body_GetPosition(cursor->body);
            auto gameObject = new Entity
                    (
                            physics.getWorld(),
                            b2Vec2{cursorPosition.x - 0.2f, cursorPosition.y + 0.2f},
                            b2Vec2{cursorPosition.x + 0.2f, cursorPosition.y - 0.2f},
                            BodyType::Dynamic
                    );

            gameObject->addComponent(new RigidBody(*gameObject, 1.5f));
            gameObject->addComponent(new SphereRenderer(*gameObject, 1.0f, 32, 16));
            gameObject->initialize();
            entities.push_back(gameObject);
        }
    }

    spawnButtonWasPressed = isPressed;
}

/// Update all entities.
///
/// The order matters. Components only ever write intent (a force, a target
/// velocity, a script decision); the world is then stepped once, as a whole;
/// and only afterwards is the result published to the render transforms. The
/// previous version stepped the world once per entity, inside the per-entity
/// update, so a scene with ten falling objects ran ten times too fast.
void World::update(GLFWwindow *window, float deltaTime) {
    handleSpawnInput(window);

    for (auto entity: entities)
        entity->update(window, deltaTime);

    physics.step(deltaTime);

    for (auto entity: entities)
        entity->syncTransform();
}

/// Render all entities
void World::render(GLFWwindow *window) {
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (auto entity: entities)
        entity->render();

    glfwSwapBuffers(window);
}

void World::setEntityName(Entity *entity, const std::string &name) {
    names.insert(std::pair<std::string, Entity *>(name, entity));
}

void World::removeEntityName(Entity *entity, const std::string &name) {
    names.erase(name);
}

Entity *World::getEntityWithName(const std::string &name) const {
    auto it = names.find(name);

    if (it == names.end())
        return nullptr;
    else
        return it->second;
}

void World::addEntityTag(Entity *entity, const std::string &tag) {
    auto it = tags.find(tag);

    if (it == tags.end()) {
        auto inserted = tags.insert(std::pair < std::string, std::list < Entity * >> ());
        it = inserted.first;
    }

    it->second.push_back(entity);
}

void World::removeEntityTag(Entity *entity, const std::string &tag) {
    auto it = tags.find(tag);

    if (it == tags.end())
        return;

    auto entities = it->second;
    auto entityIt = std::find(entities.begin(), entities.end(), entity);

    if (entityIt == entities.end())
        return;

    entities.erase(entityIt);
}

std::list<Entity*> World::getEntitiesWithTag(const std::string &tag) const {
    auto it = tags.find(tag);

    if (it == tags.end())
        return {};
    else
        return it->second;
}

Entity *World::getEntityWithTag(const std::string &tag) const {
    auto entities = getEntitiesWithTag(tag);
    auto it = entities.begin();

    if (it == entities.end())
        return nullptr;
    else
        return *it;
}

int World::getLevel() {
    return state.level;
}