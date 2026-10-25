#include "DoomController.h"
#include "Levels.h"
#include "World.h"
#include "Audio/Audio.h"
#include "Log/msg.h"
#include <algorithm>
#include <cmath>
#include <random>

using namespace doom;

namespace {

/// Radians a second the arrow keys turn, walking.
constexpr float TURN_SPEED = 2.6f;
/// Radians per pixel of mouse movement.
constexpr float MOUSE_SENSITIVITY = 0.0022f;
/// Ticks one frame may run to catch up. Below that the game slows down
/// rather than spending ever longer catching up.
constexpr int MAX_TICKS_PER_FRAME = 4;
constexpr float MUSIC_VOLUME = 0.35f;

Art loadArt(const std::string& directory)
{
    Art art = createArt();
    if (!directory.empty())
        printf("%s \bINFO: Replaced %d pictures with images from %s\n", INFO,
               loadArtOverrides(art, directory), directory.c_str());
    return art;
}

} // namespace

bool DoomController::Key::down(GLFWwindow* window) const
{
    return std::any_of(codes.begin(), codes.end(), [window](int code) { return glfwGetKey(window, code) == GLFW_PRESS; });
}

bool DoomController::Key::pressed(GLFWwindow* window)
{
    const bool wasHeld = held;
    held = down(window);
    return held && !wasHeld;
}

DoomController::DoomController(Entity& owner, const std::string& artDirectory)
    : Component(owner),
      m_Game(campaign(), std::random_device{}()),
      m_Art(loadArt(artDirectory)),
      m_Renderer(m_Art),
      m_Sounds(createDoomSounds()),
      m_Frame(Renderer::WIDTH, Renderer::HEIGHT),
      m_Canvas(Renderer::WIDTH, Renderer::HEIGHT)
{
    for (int key: {GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5})
        m_WeaponKeys.push_back(Key{key});

    Audio::getInstance().playMusic(m_Sounds.music, MUSIC_VOLUME);
}

void DoomController::update(GLFWwindow* window, float deltaTime)
{
    captureMouse(window);

    if (m_ToggleAutomap.pressed(window))
        m_Automap = !m_Automap;
    if (m_ToggleMusic.pressed(window))
    {
        m_Music = !m_Music;
        if (m_Music)
            Audio::getInstance().playMusic(m_Sounds.music, MUSIC_VOLUME);
        else
            Audio::getInstance().stopMusic();
    }

    Input input = readInput(window, deltaTime);

    m_Accumulator = std::min(m_Accumulator + deltaTime, TICK * MAX_TICKS_PER_FRAME);
    while (m_Accumulator >= TICK)
    {
        m_Accumulator -= TICK;
        m_Game.tick(input);

        // Once is enough for these: the rest of the frame's ticks only move.
        input.turn = 0.f;
        input.use = false;
        input.select = Weapon::Count;
        input.cycle = 0;
    }

    playSounds();
    m_Renderer.render(m_Game, m_Frame, m_Automap);
    m_Canvas.upload(m_Frame);
    refreshTitle(window);
}

void DoomController::render() const
{
    m_Canvas.draw({0.f, 0.f}, {(float)Renderer::WIDTH, (float)Renderer::HEIGHT});
}

/// Hide the pointer and take the mouse for looking round, while the window
/// has focus.
void DoomController::captureMouse(GLFWwindow* window)
{
    const bool focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
    if (focused == m_MouseCaptured)
        return;

    m_MouseCaptured = focused;
    if (focused)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        // Start measuring from here, so the jump to the captured pointer's
        // position does not spin the view.
        glfwGetCursorPos(window, &m_MouseX, &m_MouseY);
    }
}

Input DoomController::readInput(GLFWwindow* window, float deltaTime)
{
    auto held = [window](std::initializer_list<int> keys) {
        return std::any_of(keys.begin(), keys.end(), [window](int key) { return glfwGetKey(window, key) == GLFW_PRESS; });
    };

    Input input;
    input.run = held({GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT});
    input.forward = (float)held({GLFW_KEY_W, GLFW_KEY_UP}) - (float)held({GLFW_KEY_S, GLFW_KEY_DOWN});
    input.strafe = (float)held({GLFW_KEY_D}) - (float)held({GLFW_KEY_A});
    input.fire = held({GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL})
                 || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    input.use = m_Use.pressed(window);

    const float keyTurn = (float)held({GLFW_KEY_RIGHT}) - (float)held({GLFW_KEY_LEFT});
    input.turn = keyTurn * TURN_SPEED * (input.run ? 1.5f : 1.f) * deltaTime;

    if (m_MouseCaptured)
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        input.turn += (float)(x - m_MouseX) * MOUSE_SENSITIVITY;
        m_MouseX = x;
        m_MouseY = y;
    }

    for (int slot = 0; slot < (int)m_WeaponKeys.size(); ++slot)
        if (m_WeaponKeys[slot].pressed(window))
            input.select = (Weapon)slot;

    // Scrolling down picks the next weapon, up the one before. Touchpads
    // scroll in fractions, so those add up to whole steps first.
    m_ScrollCarry -= World::getInstance().getScroll().y;
    input.cycle = (int)m_ScrollCarry;
    m_ScrollCarry -= (float)input.cycle;

    return input;
}

void DoomController::playSounds()
{
    const Player& player = m_Game.player();
    // Each sound a little higher or lower, so the same one over and over
    // does not grate.
    static std::mt19937 random(std::random_device{}());
    std::uniform_real_distribution<float> pitch(0.94f, 1.06f);

    for (const SoundEvent& event: m_Game.takeSounds())
    {
        const SoundClipPtr& clip = m_Sounds.clipFor(event);
        if (!clip)
            continue;

        // Quieter the farther away it is.
        const float distance = std::hypot(event.x - player.x, event.y - player.y);
        const float volume = std::min(1.f, 1.4f / (1.f + distance * 0.25f));
        if (volume < 0.04f)
            continue;
        Audio::getInstance().play(clip, volume, pitch(random));
    }
}

void DoomController::refreshTitle(GLFWwindow* window)
{
    const Player& player = m_Game.player();
    const LevelStats& stats = m_Game.stats();
    std::string title = "Maple Doom - " + m_Game.map().name() + " - Health " + std::to_string(player.health)
                        + "% - Kills " + std::to_string(stats.kills) + "/" + std::to_string(stats.totalKills);

    if (title != m_Title)
    {
        m_Title = std::move(title);
        glfwSetWindowTitle(window, m_Title.c_str());
    }
}
