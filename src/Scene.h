#ifndef MAPLEENGINE_SCENE_H
#define MAPLEENGINE_SCENE_H

#include "World.h"
#include "Util/SceneManager.h"

class Scene : World {
public:
    static World& getInstance();
    static b2Vec2 getSize() { return b2Vec2{10.f, 7.5f}; }
protected:
    /// Game life cycle
    virtual void initialize() { SceneManager::getCurrentScene; }
    virtual void update(GLFWwindow* window, float deltaTime) const;
    virtual void render(GLFWwindow* window) const;

    /// Get 2D physics (in progress)
    virtual Physics& getPhysics() { return physics; }

    /// Entity handling
    virtual void setEntityName(Entity* entity, const std::string& name);
    virtual void removeEntityName(Entity* entity, const std::string& name);
    virtual Entity* getEntityWithName(const std::string& name) const;
    virtual void addEntityTag(Entity* entity, const std::string& tag);
    virtual void removeEntityTag(Entity* entity, const std::string& tag);
    virtual std::list<Entity*> getEntitiesWithTag(const std::string& tag) const;
    virtual Entity* getEntityWithTag(const std::string& tag) const;
private:
    static World gameInstance;
    std::vector<Entity*> entities;
    std::map<std::string, Entity*> names;
    std::map<std::string, std::list<Entity*>> tags;
    Physics physics;
    Scene();
};

#endif //MAPLEENGINE_WORLD_H