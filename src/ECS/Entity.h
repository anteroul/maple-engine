#ifndef MAPLEENGINE_ENTITY_H
#define MAPLEENGINE_ENTITY_H

#include <list>
#include <set>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <box2d/box2d.h>

struct Transform {
    float x, y;
    float scale_x, scale_y;
    float rotation;
};

struct Velocity {
    float x, y;
};

/// How the solver is allowed to move an entity's body.
///  - Static:    never moves. Terrain, walls, the ground.
///  - Kinematic: moved by script through its velocity, unaffected by
///               collisions and gravity, but pushes dynamic bodies.
///  - Dynamic:   fully simulated. Gravity, contacts and restitution apply.
enum class BodyType {
    Static,
    Kinematic,
    Dynamic
};

/// Surface properties of an entity's shape.
struct PhysicsMaterial {
    /// Mass per unit area, kg/m^2. Only meaningful for dynamic bodies.
    float density = 1.f;
    /// Coulomb friction coefficient, 0 is ice.
    float friction = 0.3f;
    /// Bounciness, 0 is a dead stop and 1 is a perfectly elastic bounce.
    float restitution = 0.4f;
    /// A sensor reports overlaps but never pushes anything. Use it for
    /// triggers and for cosmetic entities such as the cursor.
    bool isSensor = false;
};

class Component;

class Entity {
public:
    ///
    /// \param [in] world Reference to game world
    /// \param [in] topLeft Top left corner of the entity
    /// \param [in] bottomRight Bottom right corner of the entity
    /// \param [in] type How the solver may move this entity
    /// \param [in] material Surface properties of the entity's shape
    Entity(b2WorldId world, b2Vec2 topLeft, b2Vec2 bottomRight,
           BodyType type = BodyType::Static,
           const PhysicsMaterial& material = PhysicsMaterial{});
    ~Entity();

    /// Transform data, published from the body by syncTransform()
    struct Transform transform{};
    struct Velocity velocity{};

    /// Entity life cycle
    void initialize();
    void update(GLFWwindow* window, float deltaTime);
    /// Copy the solver's result into transform and velocity. Called once per
    /// frame after the world has been stepped, so that rendering and gameplay
    /// read one consistent snapshot of the body.
    void syncTransform();
    void render() const;
    /// Component management
    template<typename T>
    T* getComponent() const {
        for (auto component : m_Components)
        {
            T* componentCast = dynamic_cast<T*>(component);
            if (componentCast != nullptr)
                return componentCast;
        }
        return nullptr;
    }
    void addComponent(Component* component);

    /// Entity registration
    void setName(const std::string& name);
    const std::string& getName() const;
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);

    BodyType getBodyType() const { return m_BodyType; }

    /// Half the width and half the height of the entity's box, which is what
    /// Box2D's b2MakeBox takes. The full size is twice this.
    b2Vec2 halfExtents;
    b2BodyId body;
private:
    b2BodyId createBoxBody(b2WorldId world, b2Vec2 origin, b2Vec2 extents,
                           BodyType type, const PhysicsMaterial& material);
    BodyType m_BodyType;
    std::string m_Name;
    std::set<std::string> m_Tags;
    std::list<Component*> m_Components;
};


#endif //MAPLEENGINE_ENTITY_H
