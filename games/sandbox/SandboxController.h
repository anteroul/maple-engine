#ifndef MAPLESANDBOX_SANDBOXCONTROLLER_H
#define MAPLESANDBOX_SANDBOXCONTROLLER_H

#include "Simulation.h"
#include "CellColourer.h"
#include "ECS/Component.h"
#include "Effects/ParticleSystem.h"
#include "Rendering/PixelCanvas.h"
#include <initializer_list>
#include <string>
#include <vector>

namespace sandbox::layout {

/// The simulation is GRID_WIDTH x GRID_HEIGHT cells, one world unit each,
/// with the material palette in a band HUD_HEIGHT cells tall above it.
constexpr int GRID_WIDTH = 320;
constexpr int GRID_HEIGHT = 180;
constexpr int HUD_HEIGHT = 12;
constexpr int PIXELS_PER_CELL = 4;

} // namespace sandbox::layout

/// Runs the falling sand simulation and lets the player play with it: paint
/// materials, set things alight, plant torches and blow things up.
///
/// Fire and smoke come in two kinds here. In the simulation they are cells,
/// which spread, burn and drift pixel by pixel. On top of that, torches,
/// explosions and the embers off burning cells are particle effects.
class SandboxController : public Component {
public:
    explicit SandboxController(Entity& owner);

    void update(GLFWwindow* window, float deltaTime) override;
    void render() const override;

private:
    /// Edge detection over a set of keys that all do the same thing.
    struct Key {
        std::vector<int> codes;
        bool held = false;

        Key(std::initializer_list<int> keys) : codes(keys) {}
        /// True on the frame any of the keys goes down.
        bool pressed(GLFWwindow* window);
    };

    /// A torch on the wall: a particle flame that also sets alight whatever
    /// burnable cells it touches, until water puts it out.
    struct Torch {
        glm::vec2 position;
        ParticleSystem fire;
        ParticleSystem smoke;
        ParticleSystem embers;
        bool lit = true;
    };

    void reset();
    void handleInput(GLFWwindow* window);
    void stepSimulation(float deltaTime);
    void paintStroke();
    void placeTorch(glm::vec2 position);
    void removeTorchesNear(glm::vec2 position, float radius);
    void updateTorches();
    void showEvents();
    void refreshCanvas();
    void refreshTitle(GLFWwindow* window);

    void drawTorchSticks() const;
    void drawPalette() const;
    void drawBrush() const;

    sandbox::Simulation m_Simulation;
    PixelCanvas m_Canvas;
    CellColourer m_Colourer;

    std::vector<Torch> m_Torches;
    ParticleSystem m_BlastFire;
    ParticleSystem m_BlastSmoke;
    ParticleSystem m_FireSmoke;
    ParticleSystem m_Sparks;
    ParticleSystem m_Embers;

    int m_Selected = 0;
    /// Scrolling not yet worth a whole step through the palette.
    float m_ScrollCarry = 0.f;
    int m_BrushRadius = 4;
    bool m_Paused = false;
    float m_Accumulator = 0.f;
    std::uint32_t m_Frame = 0;

    glm::vec2 m_Cursor{-100.f};
    /// Where the last brush stroke ended, so a fast drag paints a line rather
    /// than a trail of dots.
    glm::vec2 m_LastPaint{0.f};
    bool m_Painting = false;
    bool m_Erasing = false;

    Key m_Torch{GLFW_KEY_T};
    Key m_Explode{GLFW_KEY_E};
    bool m_MiddleHeld = false;
    Key m_BrushSmaller{GLFW_KEY_LEFT_BRACKET, GLFW_KEY_MINUS, GLFW_KEY_KP_SUBTRACT};
    Key m_BrushLarger{GLFW_KEY_RIGHT_BRACKET, GLFW_KEY_EQUAL, GLFW_KEY_KP_ADD};
    Key m_Pause{GLFW_KEY_SPACE, GLFW_KEY_P};
    Key m_Reset{GLFW_KEY_R};
    Key m_Clear{GLFW_KEY_C};
    Key m_ToggleBloom{GLFW_KEY_B};
    std::vector<Key> m_MaterialKeys;

    std::string m_Title;
};

#endif //MAPLESANDBOX_SANDBOXCONTROLLER_H
