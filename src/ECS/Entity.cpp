#include "../World.h"
#include "Entity.h"
#include "Component.h"

Entity::Entity(b2WorldId world, b2Vec2 topLeft, b2Vec2 bottomRight,
               BodyType type, const PhysicsMaterial& material)
    : m_BodyType(type), m_Name("")
{
    b2Vec2 extents = 1.f/2.f * b2Abs(topLeft - bottomRight);
    b2Vec2 origin = 1.f/2.f * (topLeft + bottomRight);
    halfExtents = b2Vec2{extents.x, extents.y};
    body = createBoxBody(world, origin, extents, type, material);
    transform = {origin.x, origin.y, halfExtents.x, halfExtents.y, 0.f};
    velocity = {0.f, 0.f};
}

Entity::~Entity()
{
    for (auto component : m_Components)
        delete component;

    // Guarded rather than unconditional: an entity can outlive the world it
    // was created in, and destroying a body whose world is already gone is
    // not safe.
    if (!B2_IS_NULL(body) && b2Body_IsValid(body))
        b2DestroyBody(body);
}

void Entity::initialize()
{
    for (auto component : m_Components)
        component->initialize();
}

/// Update all components linked to the entity.
///
/// Components only write intent here (forces, velocities, script decisions).
/// The motion itself happens when World steps the physics world, and the
/// result reaches transform and velocity through syncTransform().
/// \param window Pointer to OpenGL window context
/// \param deltaTime World frame time
void Entity::update(GLFWwindow* window, float deltaTime)
{
    for (const auto& component : m_Components)
        component->update(window, deltaTime);
}

/// Publish the body's post-step state to the render transform.
void Entity::syncTransform()
{
    if (B2_IS_NULL(body))
        return;

    const b2Vec2 position = b2Body_GetPosition(body);
    const b2Rot rotation = b2Body_GetRotation(body);
    const b2Vec2 linearVelocity = b2Body_GetLinearVelocity(body);

    transform.x = position.x;
    transform.y = position.y;
    transform.rotation = b2Rot_GetAngle(rotation);

    velocity.x = linearVelocity.x;
    velocity.y = linearVelocity.y;
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

static b2BodyType toBox2DBodyType(BodyType type)
{
    switch (type)
    {
        case BodyType::Kinematic: return b2_kinematicBody;
        case BodyType::Dynamic:   return b2_dynamicBody;
        case BodyType::Static:
        default:                  return b2_staticBody;
    }
}

/// Creates a box body for the entity
b2BodyId Entity::createBoxBody(b2WorldId world, b2Vec2 origin, b2Vec2 extents,
                               BodyType type, const PhysicsMaterial& material)
{
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = toBox2DBodyType(type);
    bodyDef.position = origin;

    b2Polygon box = b2MakeBox(extents.x, extents.y);

    b2BodyId createdBody = b2CreateBody(world, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = material.density;
    shapeDef.material.friction = material.friction;
    shapeDef.material.restitution = material.restitution;
    shapeDef.isSensor = material.isSensor;

    b2CreatePolygonShape(createdBody, &shapeDef, &box);
    return createdBody;
}
