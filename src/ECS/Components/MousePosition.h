#ifndef MAPLEENGINE_MOUSEPOSITION_H
#define MAPLEENGINE_MOUSEPOSITION_H

#include "../Component.h"

class MousePosition : public Component {
public:
    MousePosition(Entity& owner);
    void update(GLFWwindow* window, float deltaTime) override;
};


#endif //MAPLEENGINE_MOUSEPOSITION_H
