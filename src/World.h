#ifndef MAPLEENGINE_WORLD_H
#define MAPLEENGINE_WORLD_H

#include "ECS/Entity.h"
#include "Physics.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <optional>

class World {
public:
    /// Builds a scene: creates its entities and hands them to the world with
    /// addEntity(). Runs once, from initialize(), with the GL context current.
    using SceneSetup = std::function<void(World&)>;

    static World& getInstance();
    static b2Vec2 getSize() { return b2Vec2{10.f, 7.5f}; }

    /// Choose the scene to build. Call before App::Launch().
    void setScene(SceneSetup setup);
    /// Show the region [left, right] x [bottom, top] of world space,
    /// letterboxed to keep its aspect ratio. Without a view the scene is drawn
    /// in normalised device coordinates, -1 to 1 on both axes.
    void setView(float left, float right, float bottom, float top);
    void setClearColour(float r, float g, float b);

    /// Game life cycle
    void initialize();
    void update(GLFWwindow* window, float deltaTime);
    void render(GLFWwindow* window);

    /// Get 2D physics (in progress)
    Physics& getPhysics() { return physics; }

    /// Entity handling
    /// Hand a new entity to the world. It joins the update and render loops
    /// at the end of the current update, so scripts may call this while the
    /// world is iterating its entities. The world owns it from then on.
    void addEntity(Entity* entity);
    /// Delete an entity at the end of the current update. Safe to call from
    /// the entity's own components, and more than once.
    void destroyEntity(Entity* entity);
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
    std::vector<Entity*> pendingAdditions;
    std::vector<Entity*> pendingDestructions;
    std::map<std::string, Entity*> names;
    std::map<std::string, std::list<Entity*>> tags;
    Physics physics;
    SceneSetup sceneSetup;

    struct ViewBounds {
        float left, right, bottom, top;
    };
    std::optional<ViewBounds> view;
    float clearColour[3] = {0.f, 0.f, 0.f};

    World();

    void applyView(GLFWwindow* window) const;
    /// Apply the additions and destructions queued during this update.
    void flushPendingEntities();
};

#endif //MAPLEENGINE_WORLD_H