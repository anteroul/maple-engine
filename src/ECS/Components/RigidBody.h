#ifndef MAPLEENGINE_GRAVITY_H
#define MAPLEENGINE_GRAVITY_H

#include "../Component.h"
#include <vector>

class RigidBody : public Component {
public:
    RigidBody(Entity& owner, std::vector<Entity*>* entities, float mass) : Component(owner)
    {
        m_Owner = &owner;
        m_Entities = entities;
        m_Mass = mass;
    }
    void update(GLFWwindow* window, float deltaTime) override;
    float getMass() const { return m_Mass; }
private:
    Entity* m_Owner;
    std::vector<Entity*>* m_Entities;
    float m_Mass;
};


#endif //MAPLEENGINE_GRAVITY_H
