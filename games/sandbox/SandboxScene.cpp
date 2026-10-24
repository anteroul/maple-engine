#include "SandboxScene.h"
#include "SandboxController.h"

using namespace sandbox::layout;

void createSandboxScene(World& game)
{
    // One world unit per cell, with the palette above the grid.
    game.setView(0.f, (float)GRID_WIDTH, 0.f, (float)(GRID_HEIGHT + HUD_HEIGHT));
    game.setClearColour(0.025f, 0.03f, 0.045f);

    BloomSettings bloom;
    bloom.enabled = true;
    bloom.intensity = 1.2f;
    game.setBloom(bloom);

    // The controller lives on an entity of its own, out of sight. Every
    // entity carries a body, so give it a tiny sensor nobody will notice.
    PhysicsMaterial material;
    material.isSensor = true;
    auto controller = new Entity(game.getPhysics().getWorld(), b2Vec2{-10.f, -10.f},
                                 b2Vec2{-9.99f, -10.01f}, BodyType::Static, material);
    controller->setName("sandbox");
    controller->addComponent(new SandboxController(*controller));
    game.addEntity(controller);
}
