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
        m_OnFreefall = true;
        m_Speed = 0.f;
    }
    void update(GLFWwindow* window, float deltaTime) override;
    float getMass() const { return m_Mass; }
    bool onFreefall() const { return m_OnFreefall; }
    float m_Speed;
private:
    Entity* m_Owner;
    std::vector<Entity*>* m_Entities;
    float m_Mass;
    bool m_OnFreefall;
};


#endif //MAPLEENGINE_GRAVITY_H
