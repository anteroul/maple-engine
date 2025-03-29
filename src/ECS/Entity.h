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
};

struct Velocity {
    float x, y;
};

class Component;

class Entity {
public:
    ///
    /// \param [in] world Reference to game world
    /// \param [in] topLeft Top left corner of the entity
    /// \param [in] bottomRight Bottom right corner of the entity
    Entity(b2World& world, b2Vec2 topLeft, b2Vec2 bottomRight);
    ~Entity();

    /// Transform data
    struct Transform transform{};
    struct Velocity velocity{};

    /// Entity life cycle
    void initialize();
    void update(GLFWwindow* window, float deltaTime);
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
    [[nodiscard]] const std::string& getName() const;
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);

    b2Vec2 size;
    b2Body* body;
private:
    b2Body* createBoxBody(b2World& world, b2Vec2 origin, b2Vec2 extents);
    std::string m_Name;
    std::set<std::string> m_Tags;
    std::list<Component*> m_Components;
};


#endif //MAPLEENGINE_ENTITY_H
