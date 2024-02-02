#ifndef MAPLEENGINE_BEHAVIOURSCRIPT_H
#define MAPLEENGINE_BEHAVIOURSCRIPT_H

#include "../Component.h"
#include <functional>

class BehaviourScript : public Component {
public:
    BehaviourScript(Entity& owner, auto script) : Component(owner)
    {
        m_BehaviourScript = script;
    }
    void update(GLFWwindow* window, float deltaTime) override
    {
        m_BehaviourScript(this, window, deltaTime);
    }
private:
    std::function<void(Component*, GLFWwindow*, float)> m_BehaviourScript;
};


#endif //MAPLEENGINE_BEHAVIOURSCRIPT_H
