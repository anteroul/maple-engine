#ifndef MAPLEENGINE_WORLD_H
#define MAPLEENGINE_WORLD_H

#include "ECS/Entity.h"
#include "Physics.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>
#include <list>
#include <map>

class World {
public:
    static World& getInstance();
    static b2Vec2 getSize() { return b2Vec2(10., 7.5); }

    /// Game life cycle
    void initialize();
    void update(GLFWwindow* window, float deltaTime);
    void render(GLFWwindow* window);

    /// Get 2D physics (in progress)
    Physics& getPhysics() { return physics; }

    /// Entity handling
    void setEntityName(Entity* entity, const std::string& name);
    void removeEntityName(Entity* entity, const std::string& name);
    Entity* getEntityWithName(const std::string& name) const;
    void addEntityTag(Entity* entity, const std::string& tag);
    void removeEntityTag(Entity* entity, const std::string& tag);
    std::list<Entity*> getEntitiesWithTag(const std::string& tag) const;
    Entity* getEntityWithTag(const std::string& tag) const;
private:
    static World gameInstance;
    std::vector<Entity*> entities;
    std::map<std::string, Entity*> names;
    std::map<std::string, std::list<Entity*>> tags;
    Physics physics;
    World();
};

#endif //MAPLEENGINE_WORLD_H
