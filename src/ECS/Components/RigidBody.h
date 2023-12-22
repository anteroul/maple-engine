#ifndef MAPLEENGINE_GRAVITY_H
#define MAPLEENGINE_GRAVITY_H

#include "../Component.h"

class RigidBody : public Component {
public:
    RigidBody(Entity& owner, Entity& groundEntity) : Component(owner)
    {
        m_Owner = &owner;
        ground = &groundEntity;
    }
    void update(GLFWwindow* window, float deltaTime) override;
private:
    Entity* m_Owner;
    Entity* ground;
};


#endif //MAPLEENGINE_GRAVITY_H
