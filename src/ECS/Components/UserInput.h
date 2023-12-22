#ifndef MAPLEENGINE_PLAYERINPUT_H
#define MAPLEENGINE_PLAYERINPUT_H

#include "../Component.h"

class UserInput : public Component {
public:
    UserInput(Entity &owner, float speed);
    void update(GLFWwindow* window, float deltaTime) override;
private:
    float m_Speed;
};


#endif //MAPLEENGINE_PLAYERINPUT_H
