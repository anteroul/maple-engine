#include "SandboxController.h"
#include "Level.h"
#include "World.h"
#include "Effects/ParticleEffects.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace sandbox;
using namespace sandbox::layout;

namespace {

/// The simulation always runs at this rate, whatever the frame rate.
constexpr float TICK = 1.f / 60.f;
/// Ticks one frame may run to catch up. Below that the game slows down
/// rather than spending ever longer catching up.
constexpr int MAX_TICKS_PER_FRAME = 3;

constexpr int MIN_BRUSH = 1;
constexpr int MAX_BRUSH = 24;
constexpr int MAX_TORCHES = 24;
/// The stick below a torch's flame, in cells.
constexpr float TORCH_LENGTH = 9.f;
/// Explosions shown per frame. A gunpowder chain can set off hundreds.
constexpr int MAX_BLASTS_SHOWN = 6;

/// The materials the player can paint, on keys 1 to 9 and then 0, with how
/// densely the brush sprays each.
struct PaletteEntry {
    Material material;
    float density;
};

constexpr std::array<PaletteEntry, 10> PALETTE = {{
        {Material::Sand, 0.5f},
        {Material::Water, 0.6f},
        {Material::Stone, 1.f},
        {Material::Wood, 1.f},
        {Material::Oil, 0.6f},
        {Material::Gunpowder, 0.5f},
        {Material::Lava, 0.6f},
        {Material::Fire, 0.4f},
        {Material::Smoke, 0.4f},
        {Material::Steam, 0.4f},
}};

/// Cheap per-pixel noise, for flicker that differs from cell to cell.
float noise(int x, int y, std::uint32_t t)
{
    std::uint32_t h = (std::uint32_t)x * 374761393u + (std::uint32_t)y * 668265263u + t * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.f;
}

/// What a cell looks like. Fire, lava and burning cells are brighter than
/// white, which is what makes them glow with bloom on.
///
/// shaders/cells.comp does the same on the GPU: change one, change the other.
glm::vec4 cellColour(const Cell& cell, int x, int y, std::uint32_t frame)
{
    const float shade = (float)cell.shade / 255.f;
    const float flicker = noise(x, y, frame / 3);
    const float life = (float)cell.life;
    glm::vec3 rgb{0.f};
    float alpha = 1.f;

    switch (cell.material)
    {
        case Material::Empty:
            return glm::vec4{0.f};
        case Material::Stone:
            rgb = glm::vec3{0.33f, 0.33f, 0.36f} * (0.8f + 0.35f * shade);
            break;
        case Material::Sand:
            rgb = glm::vec3{0.86f, 0.71f, 0.42f} * (0.82f + 0.25f * shade);
            break;
        case Material::Water:
            rgb = glm::vec3{0.13f, 0.33f, 0.78f} * (0.9f + 0.15f * shade);
            alpha = 0.85f;
            break;
        case Material::Wood:
            rgb = glm::vec3{0.40f, 0.24f, 0.12f} * (0.75f + 0.4f * shade);
            break;
        case Material::Oil:
            rgb = glm::vec3{0.22f, 0.15f, 0.07f} * (0.8f + 0.35f * shade);
            alpha = 0.95f;
            break;
        case Material::Gunpowder:
            rgb = glm::vec3{0.22f, 0.22f, 0.25f} * (0.6f + 0.7f * shade);
            break;
        case Material::Lava:
        {
            const float wave = 0.5f + 0.5f * std::sin((float)frame * 0.05f + shade * 6.283f);
            rgb = glm::mix(glm::vec3{2.4f, 0.5f, 0.06f}, glm::vec3{3.6f, 1.4f, 0.25f}, wave);
            break;
        }
        case Material::Fire:
        {
            const float heat = std::clamp(life / 30.f, 0.f, 1.f);
            rgb = glm::mix(glm::vec3{2.0f, 0.3f, 0.04f}, glm::vec3{4.5f, 2.6f, 0.8f}, heat)
                  * (0.7f + 0.6f * flicker);
            break;
        }
        case Material::Smoke:
            rgb = glm::vec3{0.34f, 0.33f, 0.34f} * (0.85f + 0.3f * shade);
            alpha = 0.25f + 0.5f * std::clamp(life / 200.f, 0.f, 1.f);
            break;
        case Material::Steam:
            rgb = glm::vec3{0.75f, 0.8f, 0.86f};
            alpha = 0.12f + 0.35f * std::clamp(life / 200.f, 0.f, 1.f);
            break;
        case Material::Ash:
            rgb = glm::vec3{0.46f, 0.44f, 0.42f} * (0.8f + 0.4f * shade);
            break;
        default:
            break;
    }

    if (cell.burning)
        rgb = glm::mix(rgb, glm::vec3{3.2f, 1.1f, 0.2f} * (0.6f + 0.8f * flicker), 0.65f);

    return {rgb, alpha};
}

/// A palette swatch's colour: a cell of that material, toned down so the
/// glowing ones do not flood the palette with bloom.
glm::vec3 swatchColour(Material material, std::uint32_t frame)
{
    Cell cell{material, 128};
    cell.life = 40;
    if (material == Material::Smoke || material == Material::Steam)
        cell.life = 200;
    return glm::min(glm::vec3(cellColour(cell, 0, 0, frame)), glm::vec3{1.3f});
}

/// Top of the ground at column x: the cell above the highest solid one.
int surfaceAt(const Simulation& simulation, int x)
{
    for (int y = simulation.height() - 1; y >= 0; --y)
        if (simulation.materialAt(x, y) != Material::Empty)
            return y + 1;
    return 0;
}

} // namespace

bool SandboxController::Key::pressed(GLFWwindow* window)
{
    bool down = false;
    for (int code: codes)
        down = down || glfwGetKey(window, code) == GLFW_PRESS;

    const bool wasHeld = held;
    held = down;
    return down && !wasHeld;
}

SandboxController::SandboxController(Entity& owner)
    : Component(owner),
      m_Simulation(GRID_WIDTH, GRID_HEIGHT, std::random_device{}()),
      m_Canvas(GRID_WIDTH, GRID_HEIGHT),
      m_BlastFire(effects::explosion(5.f)),
      m_BlastSmoke(effects::smoke(5.f)),
      m_FireSmoke(effects::smoke(3.f)),
      m_Sparks(effects::sparks(3.f)),
      m_Embers(effects::embers(3.f))
{
    for (int key: {GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5,
                   GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9, GLFW_KEY_0})
        m_MaterialKeys.push_back(Key{key});

    // Blast smoke billows out rather than rising as a column.
    ParticleEffect blastSmoke = m_BlastSmoke.getEffect();
    blastSmoke.spread = 6.2831853f;
    blastSmoke.speedMax *= 3.f;
    m_BlastSmoke.setEffect(blastSmoke);

    reset();
}

void SandboxController::reset()
{
    buildStarterLevel(m_Simulation);
    m_Torches.clear();
    m_BlastFire.clear();
    m_BlastSmoke.clear();
    m_FireSmoke.clear();
    m_Sparks.clear();
    m_Embers.clear();

    // A torch outside the hut, to show what one does before the player
    // plants their own.
    const int x = GRID_WIDTH * 108 / 320;
    placeTorch(glm::vec2{(float)x + 0.5f, (float)surfaceAt(m_Simulation, x) + TORCH_LENGTH});
}

void SandboxController::update(GLFWwindow* window, float deltaTime)
{
    ++m_Frame;

    b2Vec2 cursor;
    if (World::getInstance().getCursorPosition(window, cursor))
        m_Cursor = {cursor.x, cursor.y};

    handleInput(window);

    if (m_Paused)
    {
        // Still paint while paused, to set up a scene before letting it run.
        paintStroke();
    }
    else
    {
        stepSimulation(deltaTime);
    }

    showEvents();

    const float particleTime = m_Paused ? 0.f : deltaTime;
    for (Torch& torch: m_Torches)
    {
        if (torch.lit)
        {
            torch.fire.emit(torch.position, particleTime);
            torch.embers.emit(torch.position, particleTime);
        }
        torch.smoke.emit(torch.position + glm::vec2{0.f, 5.f}, particleTime * (torch.lit ? 1.f : 0.3f));
        torch.fire.update(particleTime);
        torch.smoke.update(particleTime);
        torch.embers.update(particleTime);
    }
    m_BlastFire.update(particleTime);
    m_BlastSmoke.update(particleTime);
    m_FireSmoke.update(particleTime);
    m_Sparks.update(particleTime);
    m_Embers.update(particleTime);

    refreshCanvas();
    refreshTitle(window);
}

void SandboxController::handleInput(GLFWwindow* window)
{
    for (int i = 0; i < (int)m_MaterialKeys.size(); ++i)
        if (m_MaterialKeys[i].pressed(window))
            m_Selected = i;

    // Scrolling down moves along the palette, up moves back, wrapping round.
    // Touchpads scroll in fractions, so those add up to whole steps first.
    m_ScrollCarry -= World::getInstance().getScroll().y;
    const int steps = (int)m_ScrollCarry;
    m_ScrollCarry -= (float)steps;
    const int materials = (int)PALETTE.size();
    m_Selected = ((m_Selected + steps) % materials + materials) % materials;

    if (m_BrushSmaller.pressed(window))
        m_BrushRadius = std::max(MIN_BRUSH, m_BrushRadius - 1);
    if (m_BrushLarger.pressed(window))
        m_BrushRadius = std::min(MAX_BRUSH, m_BrushRadius + 1);

    if (m_Pause.pressed(window))
        m_Paused = !m_Paused;
    if (m_Reset.pressed(window))
        reset();
    if (m_Clear.pressed(window))
    {
        m_Simulation.clear();
        m_Torches.clear();
    }
    if (m_ToggleBloom.pressed(window))
    {
        BloomSettings bloom = World::getInstance().getBloom();
        bloom.enabled = !bloom.enabled;
        World::getInstance().setBloom(bloom);
    }

    const bool onGrid = m_Cursor.x >= 0.f && m_Cursor.x < (float)GRID_WIDTH
                        && m_Cursor.y >= 0.f && m_Cursor.y < (float)GRID_HEIGHT;

    if (m_Torch.pressed(window) && onGrid)
        placeTorch(m_Cursor);

    const bool middle = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    const bool middleClicked = middle && !m_MiddleHeld;
    m_MiddleHeld = middle;
    if ((m_Explode.pressed(window) || middleClicked) && onGrid)
        m_Simulation.explode((int)m_Cursor.x, (int)m_Cursor.y, 8 + m_BrushRadius);

    const bool painting = onGrid && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool erasing = onGrid && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if ((painting || erasing) && !(m_Painting || m_Erasing))
        m_LastPaint = m_Cursor;
    m_Painting = painting;
    m_Erasing = erasing && !painting;

    if (m_Erasing)
        removeTorchesNear(m_Cursor, (float)m_BrushRadius + 1.f);
}

void SandboxController::stepSimulation(float deltaTime)
{
    m_Accumulator = std::min(m_Accumulator + deltaTime, TICK * MAX_TICKS_PER_FRAME);

    while (m_Accumulator >= TICK)
    {
        m_Accumulator -= TICK;
        // Painting once per tick rather than once per frame keeps the spray
        // just as dense at any frame rate.
        paintStroke();
        updateTorches();
        m_Simulation.step();
    }
}

void SandboxController::paintStroke()
{
    if (!m_Painting && !m_Erasing)
        return;

    const PaletteEntry& entry = PALETTE[m_Selected];
    const Material material = m_Erasing ? Material::Empty : entry.material;
    const float density = m_Erasing ? 1.f : entry.density;

    // Stamp along the line from where the last stroke ended, half a brush
    // apart, so a quick flick of the mouse leaves no gaps.
    const glm::vec2 delta = m_Cursor - m_LastPaint;
    const float spacing = std::max(1.f, (float)m_BrushRadius / 2.f);
    const int steps = std::max(1, (int)std::ceil(glm::length(delta) / spacing));

    for (int i = 1; i <= steps; ++i)
    {
        const glm::vec2 point = m_LastPaint + delta * ((float)i / (float)steps);
        m_Simulation.paint((int)point.x, (int)point.y, m_BrushRadius, material, density);
    }
    m_LastPaint = m_Cursor;
}

void SandboxController::placeTorch(glm::vec2 position)
{
    if ((int)m_Torches.size() >= MAX_TORCHES)
        m_Torches.erase(m_Torches.begin());

    std::random_device seed;
    m_Torches.push_back(Torch{position,
                              ParticleSystem(effects::fire(3.f), seed()),
                              ParticleSystem(effects::smoke(2.5f), seed()),
                              ParticleSystem(effects::embers(2.f), seed())});
}

void SandboxController::removeTorchesNear(glm::vec2 position, float radius)
{
    std::erase_if(m_Torches, [&](const Torch& torch) {
        // Anywhere along the stick counts, not just the flame.
        const glm::vec2 nearest{torch.position.x,
                                std::clamp(position.y, torch.position.y - TORCH_LENGTH, torch.position.y)};
        return glm::length(nearest - position) <= radius;
    });
}

void SandboxController::updateTorches()
{
    for (Torch& torch: m_Torches)
    {
        if (!torch.lit)
            continue;

        const int x = (int)torch.position.x;
        const int y = (int)torch.position.y;

        // Water douses a torch for good, with a hiss of steam.
        if (m_Simulation.materialAt(x, y) == Material::Water || m_Simulation.materialAt(x, y + 1) == Material::Water)
        {
            torch.lit = false;
            m_Simulation.paint(x, y + 2, 2, Material::Steam, 0.6f);
            continue;
        }

        // The flame sets alight whatever burns in it: a few cells wide at
        // the base, narrowing towards the tip.
        for (int dy = -1; dy <= 7; ++dy)
            for (int dx = -2 + std::max(dy, 0) / 4; dx <= 2 - std::max(dy, 0) / 4; ++dx)
                if ((m_Frame + dx + dy) % 4 == 0)
                    m_Simulation.ignite(x + dx, y + dy);
    }
}

void SandboxController::showEvents()
{
    std::mt19937 random(m_Frame);
    auto offset = [&random](float radius) {
        std::uniform_real_distribution<float> spread(-radius, radius);
        return glm::vec2{spread(random), spread(random)};
    };

    const std::vector<Explosion> blasts = m_Simulation.takeExplosions();
    for (int i = 0; i < std::min((int)blasts.size(), MAX_BLASTS_SHOWN); ++i)
    {
        const Explosion& blast = blasts[i];
        const glm::vec2 centre{(float)blast.x + 0.5f, (float)blast.y + 0.5f};
        const float radius = (float)blast.radius;

        // Big blasts are several fireballs, so they fill their radius.
        const int fireballs = 1 + blast.radius / 5;
        for (int f = 0; f < fireballs; ++f)
            m_BlastFire.burst(centre + offset(radius * 0.4f), 10 + blast.radius * 2);
        m_Sparks.burst(centre, 8 + blast.radius * 3);
        m_BlastSmoke.burst(centre + offset(radius * 0.3f), 2 + blast.radius / 2);
    }

    // A few embers and wisps of smoke off whatever just caught fire, which
    // soften the pixel smoke the simulation makes.
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    for (const Point& point: m_Simulation.takeIgnitions())
    {
        const glm::vec2 position{(float)point.x + 0.5f, (float)point.y + 0.5f};
        if (unit(random) < 0.05f)
            m_Embers.burst(position, 1);
        if (unit(random) < 0.08f)
            m_FireSmoke.burst(position + glm::vec2{0.f, 3.f}, 1);
    }
}

void SandboxController::refreshCanvas()
{
    if (m_Colourer.colour(m_Simulation, m_Frame, m_Canvas))
        return;

    // No compute shaders: colour every cell here instead.
    for (int y = 0; y < GRID_HEIGHT; ++y)
        for (int x = 0; x < GRID_WIDTH; ++x)
            m_Canvas.setPixel(x, y, cellColour(m_Simulation.at(x, y), x, y, m_Frame));
}

void SandboxController::refreshTitle(GLFWwindow* window)
{
    std::string title = "Maple Sandbox  |  " + std::string(info(PALETTE[m_Selected].material).name)
                        + "  |  brush " + std::to_string(m_BrushRadius);
    if (m_Paused)
        title += "  |  PAUSED";
    if (!World::getInstance().getBloom().enabled)
        title += "  |  bloom off";

    if (title != m_Title)
    {
        m_Title = title;
        glfwSetWindowTitle(window, m_Title.c_str());
    }
}

void SandboxController::render() const
{
    m_Canvas.draw({0.f, 0.f}, {(float)GRID_WIDTH, (float)GRID_HEIGHT});

    drawTorchSticks();

    // Smoke first, so that flames show through it.
    for (const Torch& torch: m_Torches)
        torch.smoke.render();
    m_BlastSmoke.render();
    m_FireSmoke.render();

    for (const Torch& torch: m_Torches)
    {
        torch.fire.render();
        torch.embers.render();
    }
    m_BlastFire.render();
    m_Sparks.render();
    m_Embers.render();

    drawPalette();
    drawBrush();
}

void SandboxController::drawTorchSticks() const
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBegin(GL_QUADS);
    for (const Torch& torch: m_Torches)
    {
        const glm::vec2 p = torch.position;
        glColor3f(0.32f, 0.19f, 0.09f);
        glVertex2f(p.x - 0.8f, p.y - TORCH_LENGTH);
        glVertex2f(p.x + 0.8f, p.y - TORCH_LENGTH);
        glVertex2f(p.x + 0.8f, p.y);
        glVertex2f(p.x - 0.8f, p.y);

        // The head: glowing coals while lit, charred once out.
        if (torch.lit)
            glColor3f(2.5f, 0.8f, 0.15f);
        else
            glColor3f(0.12f, 0.1f, 0.09f);
        glVertex2f(p.x - 1.2f, p.y - 1.5f);
        glVertex2f(p.x + 1.2f, p.y - 1.5f);
        glVertex2f(p.x + 1.2f, p.y + 0.5f);
        glVertex2f(p.x - 1.2f, p.y + 0.5f);
    }
    glEnd();
}

void SandboxController::drawPalette() const
{
    constexpr float SWATCH_WIDTH = 14.f;
    constexpr float SWATCH_HEIGHT = 7.f;
    constexpr float GAP = 4.f;
    constexpr float LEFT = 6.f;
    const float top = (float)GRID_HEIGHT;
    const float swatchBottom = top + ((float)HUD_HEIGHT - SWATCH_HEIGHT) / 2.f;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_QUADS);
    glColor3f(0.06f, 0.06f, 0.08f);
    glVertex2f(0.f, top);
    glVertex2f((float)GRID_WIDTH, top);
    glVertex2f((float)GRID_WIDTH, top + (float)HUD_HEIGHT);
    glVertex2f(0.f, top + (float)HUD_HEIGHT);

    for (int i = 0; i < (int)PALETTE.size(); ++i)
    {
        const float x = LEFT + (float)i * (SWATCH_WIDTH + GAP);
        // The selected swatch sits a little higher.
        const float y = swatchBottom + (i == m_Selected ? 1.f : 0.f);
        const glm::vec3 colour = swatchColour(PALETTE[i].material, m_Frame);
        glColor3f(colour.r, colour.g, colour.b);
        glVertex2f(x, y);
        glVertex2f(x + SWATCH_WIDTH, y);
        glVertex2f(x + SWATCH_WIDTH, y + SWATCH_HEIGHT);
        glVertex2f(x, y + SWATCH_HEIGHT);
    }

    // Pause sign at the far right.
    if (m_Paused)
    {
        glColor3f(0.9f, 0.9f, 0.9f);
        for (float x: {(float)GRID_WIDTH - 12.f, (float)GRID_WIDTH - 7.f})
        {
            glVertex2f(x, swatchBottom);
            glVertex2f(x + 3.f, swatchBottom);
            glVertex2f(x + 3.f, swatchBottom + SWATCH_HEIGHT);
            glVertex2f(x, swatchBottom + SWATCH_HEIGHT);
        }
    }
    glEnd();

    // Outline the selected swatch.
    const float x = LEFT + (float)m_Selected * (SWATCH_WIDTH + GAP);
    const float y = swatchBottom + 1.f;
    glLineWidth(2.f);
    glColor3f(1.f, 1.f, 1.f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x - 0.5f, y - 0.5f);
    glVertex2f(x + SWATCH_WIDTH + 0.5f, y - 0.5f);
    glVertex2f(x + SWATCH_WIDTH + 0.5f, y + SWATCH_HEIGHT + 0.5f);
    glVertex2f(x - 0.5f, y + SWATCH_HEIGHT + 0.5f);
    glEnd();
    glLineWidth(1.f);
}

void SandboxController::drawBrush() const
{
    if (m_Cursor.x < 0.f || m_Cursor.x >= (float)GRID_WIDTH
        || m_Cursor.y < 0.f || m_Cursor.y >= (float)GRID_HEIGHT)
        return;

    // The window hides the system cursor, so this is the only one there is.
    const glm::vec3 colour = m_Erasing ? glm::vec3{1.f, 0.35f, 0.35f}
                                       : glm::clamp(swatchColour(PALETTE[m_Selected].material, m_Frame)
                                                    + 0.3f, 0.f, 1.f);
    const float radius = (float)m_BrushRadius + 0.5f;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3f(colour.r, colour.g, colour.b);

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 40; ++i)
    {
        const float angle = (float)i / 40.f * 6.2831853f;
        glVertex2f(m_Cursor.x + radius * std::cos(angle), m_Cursor.y + radius * std::sin(angle));
    }
    glEnd();

    glBegin(GL_LINES);
    glVertex2f(m_Cursor.x - 1.5f, m_Cursor.y);
    glVertex2f(m_Cursor.x + 1.5f, m_Cursor.y);
    glVertex2f(m_Cursor.x, m_Cursor.y - 1.5f);
    glVertex2f(m_Cursor.x, m_Cursor.y + 1.5f);
    glEnd();
}
