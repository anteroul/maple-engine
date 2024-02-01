#ifndef MAPLEENGINE_BOX_COLLIDER_HPP
#define MAPLEENGINE_BOX_COLLIDER_HPP

#include "../Component.h"

class BoxCollider : public Component {
public:
    BoxCollider(Entity& owner) : Component(owner)
    {}
    void update(GLFWwindow* window, float deltaTime) override
    {}
};


#endif //MAPLEENGINE_BOX_COLLIDER_HPP
