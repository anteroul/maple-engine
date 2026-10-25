#ifndef MAPLEDOOM_DOOMCONTROLLER_H
#define MAPLEDOOM_DOOMCONTROLLER_H

#include "Art.h"
#include "DoomAudio.h"
#include "Game.h"
#include "Renderer.h"
#include "ECS/Component.h"
#include "Rendering/Image.h"
#include "Rendering/PixelCanvas.h"
#include <initializer_list>
#include <string>
#include <vector>

/// Plays Maple Doom: reads the keyboard and mouse, runs the rules in
/// doom::Game at a fixed rate, plays the sounds they make, and shows each
/// frame the software renderer draws.
class DoomController : public Component {
public:
    /// \param artDirectory Where to look for PNG files replacing the
    ///        built in art, or empty for none.
    DoomController(Entity& owner, const std::string& artDirectory);

    void update(GLFWwindow* window, float deltaTime) override;
    void render() const override;

private:
    /// Edge detection over a set of keys that all do the same thing.
    struct Key {
        std::vector<int> codes;
        bool held = false;

        Key(std::initializer_list<int> keys) : codes(keys) {}
        bool down(GLFWwindow* window) const;
        /// True on the frame any of the keys goes down.
        bool pressed(GLFWwindow* window);
    };

    /// What the player asks for this frame. Turning, using and switching
    /// weapons happen once, on the first tick of the frame.
    doom::Input readInput(GLFWwindow* window, float deltaTime);
    void captureMouse(GLFWwindow* window);
    void playSounds();
    void refreshTitle(GLFWwindow* window);

    doom::Game m_Game;
    doom::Art m_Art;
    doom::Renderer m_Renderer;
    DoomSounds m_Sounds;
    Image m_Frame;
    PixelCanvas m_Canvas;

    float m_Accumulator = 0.f;
    bool m_MouseCaptured = false;
    double m_MouseX = 0.0;
    double m_MouseY = 0.0;
    float m_ScrollCarry = 0.f;
    bool m_Automap = false;
    bool m_Music = true;

    Key m_Use{GLFW_KEY_E, GLFW_KEY_SPACE};
    Key m_ToggleAutomap{GLFW_KEY_TAB};
    Key m_ToggleMusic{GLFW_KEY_M};
    std::vector<Key> m_WeaponKeys;

    std::string m_Title;
};

#endif //MAPLEDOOM_DOOMCONTROLLER_H
