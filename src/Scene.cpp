#include "Scene.h"
#include "ECS/Components/UserInput.h"
#include "ECS/Components/BoxRenderer.h"
#include "ECS/Components/SphereRenderer.h"
#include "ECS/Components/MouseFollow.h"
#include "ECS/Components/RigidBody.h"

Scene Scene::gameInstance;

Scene& Scene::getInstance()
{
    return gameInstance;
}

Scene::Scene() = default;

/// Initialize all entities
void Scene::initialize()
{
    b2World& world = physics.getWorld();

    auto ball = new Entity(world, b2Vec2(-0.2f, 0.2f), b2Vec2(0.2f, -0.2f));
    ball->setName("ball");
    ball->addTag("Player");
    ball->addComponent(new UserInput(*ball, 0.45f));
    ball->addComponent(new SphereRenderer(*ball, {0.f, 1.f, 1.f}));

    auto cursor = new Entity(world, b2Vec2(-0.01f, -0.01f), b2Vec2(0.01f, 0.01f));
    cursor->setName("cursor");
    cursor->addComponent(new MouseFollow(*cursor, NULL));
    cursor->addComponent(new BoxRenderer(*cursor, {1.f, 0.f, 0.f}));

    auto rec = new Entity(world, b2Vec2(-0.8f, 0.5f), b2Vec2(-0.5f, 0.2f));
    rec->setName("rectangle");
    rec->addComponent(new BoxRenderer(*rec, {0.f, 1.f, 0.f}));

    auto another_rec = new Entity(world, b2Vec2(0.7f, 0.6f), b2Vec2(0.9f, 0.2f));
    another_rec->setName("rectangle");
    another_rec->addComponent(new BoxRenderer(*another_rec, {1.f, 0.5f, 0.f}));

    auto sphere = new Entity(world, b2Vec2(0.f, 0.f), b2Vec2(0.2f, 0.2f));
    sphere->addComponent(new SphereRenderer(*sphere, {1.f, 0.f, 1.f}));
    sphere->addComponent(new MouseFollow(*sphere, 0.17f));

    auto ground = new Entity(world, b2Vec2(-2.f, -0.4f), b2Vec2(2.f, -0.8f));
    ground->setName("ground");
    ground->addComponent(new BoxRenderer(*ground, {1.f, 1.f, 0.f}));
    ground->addComponent(new UserInput(*ground, 4.5f));

    ball->addComponent(new RigidBody(*ball, *ground));
    rec->addComponent(new RigidBody(*rec, *ground));
    another_rec->addComponent(new RigidBody(*another_rec, *ground));
    sphere->addComponent(new RigidBody(*sphere, *ground));

    entities.push_back(ball);
    entities.push_back(rec);
    entities.push_back(ground);
    entities.push_back(cursor);
    entities.push_back(another_rec);
    entities.push_back(sphere);

    for (auto i : entities)
        i->initialize();
}

/// Update all entites
void Scene::update(GLFWwindow* window, float deltaTime)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        auto cursor = getEntityWithName("cursor");
        auto ground = getEntityWithName("ground");

        auto gameObject = new Entity
                (
                        physics.getWorld(),
                        b2Vec2(cursor->body->GetPosition().x - 0.2f, cursor->body->GetPosition().y + 0.2f),
                        b2Vec2(cursor->body->GetPosition().x + 0.2f, cursor->body->GetPosition().y - 0.2f)
                );

        gameObject->addComponent(new RigidBody(*gameObject, *ground));
        gameObject->addComponent(new BoxRenderer(*gameObject, {1.f, 0.f, 0.f}));
        entities.push_back(gameObject);
    }

    for (auto entity : entities)
    {
        if (entity->getComponment<RigidBody>())
            physics.update(entity, deltaTime);
        entity->update(window, deltaTime);
    }
}

/// Render all entities
void Scene::render(GLFWwindow* window)
{
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (auto entity : entities)
        entity->render();

    glfwSwapBuffers(window);
}

void Scene::setEntityName(Entity* entity, const std::string& name)
{
    names.insert(std::pair<std::string, Entity*>(name, entity));
}

void Scene::removeEntityName(Entity* entity, const std::string& name)
{
    names.erase(name);
}

Entity* Scene::getEntityWithName(const std::string& name) const
{
    auto it = names.find(name);

    if (it == names.end())
        return nullptr;
    else
        return it->second;
}

void Scene::addEntityTag(Entity* entity, const std::string& tag)
{
    auto it = tags.find(tag);

    if (it == tags.end())
    {
        auto inserted = tags.insert(std::pair<std::string, std::list<Entity*>>());
        it = inserted.first;
    }

    it->second.push_back(entity);
}

void Scene::removeEntityTag(Entity* entity, const std::string& tag)
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

std::list<Entity*> Scene::getEntitiesWithTag(const std::string& tag) const
{
    auto it = tags.find(tag);

    if (it == tags.end())
        return {};
    else
        return it->second;
}

Entity* Scene::getEntityWithTag(const std::string& tag) const
{
    auto entities = getEntitiesWithTag(tag);
    auto it = entities.begin();

    if (it == entities.end())
        return nullptr;
    else
        return *it;
}
