#include "World.h"
#include "ECS/Components/UserInput.h"
#include "ECS/Components/BoxRenderer.h"
#include "ECS/Components/SphereRenderer.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/BehaviourScript.h"
#include "Scripts/MouseFollow.h"

World World::gameInstance;

World& World::getInstance()
{
    return gameInstance;
}

World::World() = default;

/// Initialize all entities
void World::initialize()
{
    b2World& world = physics.getWorld();

    auto ball = new Entity(world, b2Vec2(-0.2f, 0.2f), b2Vec2(0.2f, -0.2f));
    ball->setName("ball");
    ball->addTag("Player");
    ball->addComponent(new UserInput(*ball, 0.01f));
    ball->addComponent(new SphereRenderer(*ball, {0.f, 1.f, 1.f}));
    ball->addComponent(new RigidBody(*ball, &entities, 4.5f));

    auto cursor = new Entity(world, b2Vec2(-0.01f, -0.01f), b2Vec2(0.01f, 0.01f));
    cursor->setName("cursor");
    cursor->addComponent(new BehaviourScript(*cursor, mouseFollow));
    cursor->addComponent(new BoxRenderer(*cursor, {1.f, 0.f, 0.f}));

    auto rec = new Entity(world, b2Vec2(-0.8f, 0.6f), b2Vec2(-0.5f, 0.2f));
    rec->setName("rectangle");
    rec->addComponent(new BoxRenderer(*rec, {0.f, 1.f, 0.f}));

    auto sphere = new Entity(world, b2Vec2(0.f, 0.f), b2Vec2(0.4f, 0.4f));
    sphere->addComponent(new SphereRenderer(*sphere, {1.f, 0.f, 1.f}));
    sphere->addComponent(new BehaviourScript(*sphere, mouseFollow));

    auto ground = new Entity(world, b2Vec2(-2.f, -0.4f), b2Vec2(2.f, -0.8f));
    ground->setName("ground");
    ground->addComponent(new BoxRenderer(*ground, {1.f, 1.f, 0.f}));

    rec->addComponent(new RigidBody(*rec, &entities, 2.5f));
    sphere->addComponent(new RigidBody(*sphere, &entities, 3.0f));

    entities.push_back(ball);
    entities.push_back(rec);
    entities.push_back(ground);
    entities.push_back(cursor);
    entities.push_back(sphere);

    for (auto i : entities)
        i->initialize();
}

/// Update all entities
void World::update(GLFWwindow* window, float deltaTime)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        auto cursor = getEntityWithName("cursor");

        auto gameObject = new Entity
                (
                        physics.getWorld(),
                        b2Vec2(cursor->body->GetPosition().x - 0.2f, cursor->body->GetPosition().y + 0.2f),
                        b2Vec2(cursor->body->GetPosition().x + 0.2f, cursor->body->GetPosition().y - 0.2f)
                );

        gameObject->addComponent(new RigidBody(*gameObject, &entities, 1.5f));
        gameObject->addComponent(new BoxRenderer(*gameObject, {1.f, 0.f, 0.f}));
        entities.push_back(gameObject);
    }

    for (auto entity : entities)
    {
        if (entity->getComponent<RigidBody>())
            physics.update(entity, deltaTime);
        entity->update(window, deltaTime);
    }
}

/// Render all entities
void World::render(GLFWwindow* window)
{
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (auto entity : entities)
        entity->render();

    glfwSwapBuffers(window);
}

void World::setEntityName(Entity* entity, const std::string& name)
{
    names.insert(std::pair<std::string, Entity*>(name, entity));
}

void World::removeEntityName(Entity* entity, const std::string& name)
{
    names.erase(name);
}

Entity* World::getEntityWithName(const std::string& name) const
{
    auto it = names.find(name);

    if (it == names.end())
        return nullptr;
    else
        return it->second;
}

void World::addEntityTag(Entity* entity, const std::string& tag)
{
    auto it = tags.find(tag);

    if (it == tags.end())
    {
        auto inserted = tags.insert(std::pair<std::string, std::list<Entity*>>());
        it = inserted.first;
    }

    it->second.push_back(entity);
}

void World::removeEntityTag(Entity* entity, const std::string& tag)
{
    auto it = tags.find(tag);

    if (it == tags.end())
        return;

    auto entities = it->second;
    auto entityIt = std::find(entities.begin(), entities.end(), entity);

    if (entityIt == entities.end())
        return;

    entities.erase(entityIt);
}

std::list<Entity*> World::getEntitiesWithTag(const std::string& tag) const
{
    auto it = tags.find(tag);

    if (it == tags.end())
        return {};
    else
        return it->second;
}

Entity* World::getEntityWithTag(const std::string& tag) const
{
    auto entities = getEntitiesWithTag(tag);
    auto it = entities.begin();

    if (it == entities.end())
        return nullptr;
    else
        return *it;
}