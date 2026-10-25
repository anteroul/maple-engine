#include "DoomScene.h"
#include "DoomController.h"

void createDoomScene(World& game, const std::string& artDirectory)
{
    // One world unit per pixel of the software renderer's frame.
    game.setView(0.f, (float)doom::Renderer::WIDTH, 0.f, (float)doom::Renderer::HEIGHT);
    game.setClearColour(0.f, 0.f, 0.f);

    // The controller lives on an entity of its own, out of sight. Every
    // entity carries a body, so give it a tiny sensor nobody will notice.
    PhysicsMaterial material;
    material.isSensor = true;
    auto controller = new Entity(game.getPhysics().getWorld(), b2Vec2{-10.f, -10.f},
                                 b2Vec2{-9.99f, -10.01f}, BodyType::Static, material);
    controller->setName("doom");
    controller->addComponent(new DoomController(*controller, artDirectory));
    game.addEntity(controller);
}
