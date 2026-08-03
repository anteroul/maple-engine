#ifndef MAPLEENGINE_SCENEMANAGER_H
#define MAPLEENGINE_SCENEMANAGER_H

#include <string>
#include <vector>
#include <box2d/box2d.h>
#include <glm/glm.hpp>
#include "../ECS/Entity.h"

struct LevelBounds
{
    glm::vec2 min;
    glm::vec2 max;

    float width() const { return max.x - min.x; }
    float height() const { return max.y - min.y; }
};

struct Level
{
    std::string name;
    b2WorldId worldId = b2_nullWorldId;
    LevelBounds bounds;
    std::vector<Entity *> entities;

    // Static bodies that make up the boundary walls, so we can
    // destroy them cleanly if the level is torn down.
    std::vector<b2BodyId> boundaryBodies;
};

namespace SceneManager
{
    Level &loadScene(const std::string &name, LevelBounds bounds,
                     b2Vec2 gravity = {0.0f, -9.81f},
                     bool createBoundaryWalls = true);

    void unloadScene(const std::string &name);
    void setCurrentScene(const std::string &name);

    Level *getCurrentScene();
    Level *getScene(const std::string &name);
}

#endif