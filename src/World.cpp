#include "World.h"

World World::gameInstance;

World &World::getInstance() {
    return gameInstance;
}

World::World() = default;

void World::setScene(SceneSetup setup) {
    sceneSetup = std::move(setup);
}

void World::setView(float left, float right, float bottom, float top) {
    view = ViewBounds{left, right, bottom, top};
}

void World::setClearColour(float r, float g, float b) {
    clearColour[0] = r;
    clearColour[1] = g;
    clearColour[2] = b;
}

/// Build the scene and initialize all entities
void World::initialize() {
    if (sceneSetup)
        sceneSetup(*this);

    flushPendingEntities();

    for (auto i: entities)
        i->initialize();
}

/// Update all entities.
///
/// The order matters. Components only ever write intent (a force, a target
/// velocity, a script decision); the world is then stepped once, as a whole;
/// and only afterwards is the result published to the render transforms. The
/// previous version stepped the world once per entity, inside the per-entity
/// update, so a scene with ten falling objects ran ten times too fast.
void World::update(GLFWwindow *window, float deltaTime) {
    for (auto entity: entities)
        entity->update(window, deltaTime);

    flushPendingEntities();

    physics.step(deltaTime);

    for (auto entity: entities)
        entity->syncTransform();
}

/// Point the viewport and projection at the scene's view.
///
/// Without a view the scene draws straight in normalised device coordinates
/// over the whole framebuffer. With one, the largest viewport that keeps the
/// view's aspect ratio is centred in the window, so resizing letterboxes the
/// scene instead of stretching it.
void World::applyView(GLFWwindow *window) const {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if (!view || width <= 0 || height <= 0) {
        glViewport(0, 0, width, height);
        glMatrixMode(GL_MODELVIEW);
        return;
    }

    const float viewAspect = (view->right - view->left) / (view->top - view->bottom);
    int viewportWidth = width;
    int viewportHeight = height;

    if ((float)width / (float)height > viewAspect)
        viewportWidth = (int)((float)height * viewAspect);
    else
        viewportHeight = (int)((float)width / viewAspect);

    glViewport((width - viewportWidth) / 2, (height - viewportHeight) / 2,
               viewportWidth, viewportHeight);
    glOrtho(view->left, view->right, view->bottom, view->top, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
}

/// Render all entities
void World::render(GLFWwindow *window) {
    glClearColor(clearColour[0], clearColour[1], clearColour[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    applyView(window);

    for (auto entity: entities)
        entity->render();

    glfwSwapBuffers(window);
}

void World::addEntity(Entity *entity) {
    pendingAdditions.push_back(entity);
}

void World::destroyEntity(Entity *entity) {
    if (std::find(pendingDestructions.begin(), pendingDestructions.end(), entity)
            == pendingDestructions.end())
        pendingDestructions.push_back(entity);
}

void World::flushPendingEntities() {
    for (auto entity: pendingAdditions)
        entities.push_back(entity);
    pendingAdditions.clear();

    for (auto entity: pendingDestructions) {
        entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());

        if (!entity->getName().empty())
            removeEntityName(entity, entity->getName());

        delete entity;
    }
    pendingDestructions.clear();
}

void World::setEntityName(Entity *entity, const std::string &name) {
    names.insert(std::pair<std::string, Entity *>(name, entity));
}

void World::removeEntityName(Entity *entity, const std::string &name) {
    names.erase(name);
}

Entity *World::getEntityWithName(const std::string &name) const {
    auto it = names.find(name);

    if (it == names.end())
        return nullptr;
    else
        return it->second;
}

void World::addEntityTag(Entity *entity, const std::string &tag) {
    auto it = tags.find(tag);

    if (it == tags.end()) {
        auto inserted = tags.insert(std::pair < std::string, std::list < Entity * >> ());
        it = inserted.first;
    }

    it->second.push_back(entity);
}

void World::removeEntityTag(Entity *entity, const std::string &tag) {
    auto it = tags.find(tag);

    if (it == tags.end())
        return;

    auto entities = it->second;
    auto entityIt = std::find(entities.begin(), entities.end(), entity);

    if (entityIt == entities.end())
        return;

    entities.erase(entityIt);
}

std::list<Entity*> World::getEntitiesWithTag(const std::string &tag) const {
    auto it = tags.find(tag);

    if (it == tags.end())
        return {};
    else
        return it->second;
}

Entity *World::getEntityWithTag(const std::string &tag) const {
    auto entities = getEntitiesWithTag(tag);
    auto it = entities.begin();

    if (it == entities.end())
        return nullptr;
    else
        return *it;
}
