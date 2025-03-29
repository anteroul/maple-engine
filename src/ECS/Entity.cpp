#include "../World.h"
#include "Entity.h"
#include "Component.h"

Entity::Entity(b2World& world, b2Vec2 topLeft, b2Vec2 bottomRight) : m_Name("")
{
    b2Vec2 extents = 1.f/2.f * b2Abs(topLeft - bottomRight);
    b2Vec2 origin = 1.f/2.f * (topLeft + bottomRight);
    size = b2Vec2(extents.x, extents.y);
    body = createBoxBody(world, origin, extents);
    transform = {origin.x, origin.y, size.x, size.y, 0.f};
    velocity = {0.f, 0.f};
}

Entity::~Entity()
{
    for (auto component : m_Components)
        delete component;
}

void Entity::initialize()
{
    for (auto component : m_Components)
        component->initialize();
}

/// Update all components linked to the entity.
/// \param window Pointer to OpenGL window context
/// \param deltaTime World frame time
void Entity::update(GLFWwindow* window, float deltaTime)
{
    auto oldPos = body->GetPosition();

    for (const auto& component : m_Components)
        component->update(window, deltaTime);

    auto newPos = body->GetPosition();
    velocity.x = (newPos.x - oldPos.x) / deltaTime;
    velocity.y = (newPos.y - oldPos.y) / deltaTime;

    transform.x += velocity.x * deltaTime;
    transform.y += velocity.y * deltaTime;
}

/// Render all components linked to the entity.
void Entity::render() const
{
    for (auto component : m_Components)
        component->render();
}

void Entity::addComponent(Component *component)
{
    m_Components.push_back(component);
}

void Entity::setName(const std::string& name)
{
    World& game = World::getInstance();

    if (name.length() > 0)
    {
        if (m_Name.length() > 0)
            game.removeEntityName(this, m_Name);

        game.setEntityName(this, name);
    } else if (m_Name.length() > 0) {
        game.removeEntityName(this, m_Name);
    }

    m_Name = name;
}

const std::string& Entity::getName() const
{
    return m_Name;
}

void Entity::addTag(const std::string& tag)
{
    if (m_Tags.count(tag) > 0)
        return;

    m_Tags.insert(tag);
    World::getInstance().addEntityTag(this, tag);
}

void Entity::removeTag(const std::string& tag)
{
    auto tagIterator = m_Tags.find(tag);

    if (tagIterator == m_Tags.end())
        return;

    m_Tags.erase(tagIterator);
    World::getInstance().removeEntityTag(this, tag);
}

/// Creates a box body for the entity
b2Body* Entity::createBoxBody(b2World& world, b2Vec2 origin, b2Vec2 extents)
{
    b2BodyDef groundBodyDef;
    groundBodyDef.position = origin;

    b2PolygonShape groundBox;
    groundBox.SetAsBox(extents.x, extents.y);

    b2Body* body = world.CreateBody(&groundBodyDef);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &groundBox;
    fixtureDef.density = 1.f;
    fixtureDef.friction = 0.f;
    fixtureDef.restitution = 1.f;

    body->CreateFixture(&fixtureDef);
    return body;
}
