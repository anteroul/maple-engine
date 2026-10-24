/**
 * Maple Sandbox: a Noita-style falling sand playground built on Maple Engine.
 *
 * Controls:
 *   Left mouse              paint the selected material
 *   Right mouse             erase (torches too)
 *   1 - 9, 0                sand, water, stone, wood, oil, gunpowder, lava,
 *                           fire, smoke, steam
 *   Mouse wheel             next / previous material
 *   [ / ], - / =            brush size
 *   T                       plant a torch
 *   E, middle mouse         explosion
 *   Space, P                pause
 *   R                       reset the level
 *   C                       clear everything
 *   B                       bloom on / off
 *   Esc                     quit
 */
#include "App.h"
#include "SandboxScene.h"
#include "SandboxController.h"

int main()
{
    using namespace sandbox::layout;

    App app(GRID_WIDTH * PIXELS_PER_CELL, (GRID_HEIGHT + HUD_HEIGHT) * PIXELS_PER_CELL,
            "Maple Sandbox", false);

    World::getInstance().setScene(createSandboxScene);
    app.Launch();
    return 0;
}
