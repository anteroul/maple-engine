#pragma once

#ifndef MAPLEENGINE_APP_H
#define MAPLEENGINE_APP_H

#include "Scene.h"

class App {
public:
    App(unsigned int width, unsigned int height, char* windowTitle, bool fullscreen);
    ~App();
    void Launch();
    static void ThrowError(int error, const char* description);
private:
    void RunApplication(Scene& game);
    bool ApplicationShouldClose();
    static void HandleMouseMotion(GLFWwindow* window, double xPos, double yPos);
    double newTime{};
    double deltaTime{};
    GLFWwindow* window;
    std::vector<Scene*> scenes;
};


#endif //MAPLEENGINE_APP_H
