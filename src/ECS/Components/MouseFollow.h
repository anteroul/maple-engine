#ifndef MAPLEENGINE_MOUSEPOSITION_H
#define MAPLEENGINE_MOUSEPOSITION_H

// ATTENTION! This class is deprecated!!!
#include "../Component.h"

class MouseFollow : public Component {
public:
    MouseFollow(Entity& owner, float speed) : Component(owner) {
        m_Speed = speed;
    }
    void update(GLFWwindow* window, float deltaTime) override;
private:
    float m_Speed;
};


#endif //MAPLEENGINE_MOUSEPOSITION_H
