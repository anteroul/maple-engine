#include "World.h"
#include "ECS/Components/Glow.h"
#include "Rendering/ShaderProgram.h"

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

void World::setBloom(const BloomSettings& settings) {
    postProcess.setSettings(settings);
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

    // Every component has seen this frame's scrolling.
    scroll = b2Vec2{0.f, 0.f};
}

void World::addScroll(double x, double y) {
    scroll.x += (float)x;
    scroll.y += (float)y;
}

/// The largest viewport that keeps the view's aspect ratio, centred in the
/// framebuffer. Without a view, the whole framebuffer.
World::Viewport World::viewportFor(int width, int height) const {
    if (!view || width <= 0 || height <= 0)
        return {0, 0, width, height};

    const float viewAspect = (view->right - view->left) / (view->top - view->bottom);
    int viewportWidth = width;
    int viewportHeight = height;

    if ((float)width / (float)height > viewAspect)
        viewportWidth = (int)((float)height * viewAspect);
    else
        viewportHeight = (int)((float)width / viewAspect);

    return {(width - viewportWidth) / 2, (height - viewportHeight) / 2,
            viewportWidth, viewportHeight};
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

    const Viewport viewport = viewportFor(width, height);
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (view && width > 0 && height > 0)
        glOrtho(view->left, view->right, view->bottom, view->top, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
}

bool World::getCursorPosition(GLFWwindow *window, b2Vec2 &position) const {
    int windowWidth, windowHeight, width, height;
    double cursorX, cursorY;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &width, &height);
    glfwGetCursorPos(window, &cursorX, &cursorY);

    if (windowWidth <= 0 || windowHeight <= 0 || width <= 0 || height <= 0)
        return false;

    // The cursor is in window coordinates, measured down from the top, and
    // the viewport in framebuffer pixels, which differ on high DPI screens.
    const float x = (float)(cursorX * width / windowWidth);
    const float y = (float)((windowHeight - cursorY) * height / windowHeight);

    const Viewport viewport = viewportFor(width, height);
    const float u = (x - (float)viewport.x) / (float)viewport.width;
    const float v = (y - (float)viewport.y) / (float)viewport.height;

    if (view)
        position = {view->left + u * (view->right - view->left),
                    view->bottom + v * (view->top - view->bottom)};
    else
        position = {u * 2.f - 1.f, v * 2.f - 1.f};
    return true;
}

/// Render all entities
///
/// With bloom on, the scene goes to an offscreen buffer first. Glowing
/// entities are then drawn once more, into the glow buffer, before the two
/// are blurred together and drawn to the window.
void World::render(GLFWwindow *window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    const bool postProcessing = postProcess.beginScene(width, height);

    glClearColor(clearColour[0], clearColour[1], clearColour[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    applyView(window);

    for (auto entity: entities)
        entity->render();

    if (postProcessing) {
        postProcess.beginGlow();

        for (auto entity: entities) {
            const Glow* glow = entity->getComponent<Glow>();
            if (glow && glow->isEnabled())
                postProcess.drawGlow(*entity, *glow);
        }

        postProcess.finish();
    }

    glfwSwapBuffers(window);
}

void World::releaseRendering() {
    postProcess.release();
    // Anything still holding GL objects is destroyed after the context.
    markRenderingReleased();
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
