/**
 * Maple-Engine is a 2D game engine written in C++.
 * It uses OpenGL for rendering, Box2D for physics and also includes an entity-component-system.
 *
 * Version: 0.2.0
 * Author: Uljas Antero Lindell
 * GitHub: https://github.com/anteroul
 * @brief This is the main application where the window is initialized, as well as input, logging etc.
 */
#include "../App.h"
#include "../Scenes/DemoScene.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wwritable-strings"

int main(int argc, char* argv[])
{
    App app(800, 800, "Maple Engine", false);
    World::getInstance().setScene(createDemoScene);
    app.Launch();
    return 0;
}

#pragma clang diagnostic pop