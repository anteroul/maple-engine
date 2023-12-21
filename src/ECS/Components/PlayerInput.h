#ifndef MAPLEENGINE_PLAYERINPUT_H
#define MAPLEENGINE_PLAYERINPUT_H

#include "../../Scene.h"
#include "../Component.h"

class PlayerInput : public Component {
public:
    PlayerInput(Entity &owner, float speed);
    void update(GLFWwindow* window, float deltaTime) override;
private:
    float m_Speed;
};


#endif //MAPLEENGINE_PLAYERINPUT_H
