#ifndef MAPLEENGINE_COMPONENT_H
#define MAPLEENGINE_COMPONENT_H

#include "Entity.h"

class Component {
public:
    Component(Entity& owner) :m_Owner(owner) {}
    virtual ~Component() {}
    /// Component life cycle
    virtual void initialize() {}
    virtual void render() const {}
    virtual void update(GLFWwindow* glfWwindow, float deltaTime) {}

    Entity& getEntity() { return m_Owner; }
    void setBody(b2Body* body) { m_Owner.body = body; }
    const b2Body* getBody() const { return m_Owner.body; }
    b2Body* getBody() { return m_Owner.body; }

    template<typename T>
    T* getComponent() const { return m_Owner.getComponent<T>(); }
    void addComponent(Component *component) { return m_Owner.addComponent(component); }
private:
    Entity& m_Owner;
};

#endif //MAPLEENGINE_COMPONENT_H
