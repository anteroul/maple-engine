#ifndef MAPLEENGINE_GRAVITY_H
#define MAPLEENGINE_GRAVITY_H

#include "../Component.h"

class RigidBody : public Component {
public:
    RigidBody(Entity& owner, Entity& groundEntity, b2World& world) : Component(owner)
    {
        m_Owner = &owner;
        ground = &groundEntity;
        m_World = &world;
    }
    void update(GLFWwindow* window, float deltaTime) override;
private:
    Entity* m_Owner;
    b2World* m_World;
    Entity* ground;
};


#endif //MAPLEENGINE_GRAVITY_H
