#ifndef MAPLETETRIS_TETRISCONTROLLER_H
#define MAPLETETRIS_TETRISCONTROLLER_H

#include "TetrisGame.h"
#include "SevenSegmentDisplay.h"
#include "ECS/Component.h"
#include "ECS/Components/BoxRenderer.h"
#include <array>
#include <initializer_list>
#include <string>
#include <vector>

/// Everything on screen the controller drives. Built by createTetrisScene().
struct TetrisViews {
    /// One renderer per visible board cell, indexed [y * BOARD_WIDTH + x].
    std::array<BoxRenderer*, tetris::BOARD_WIDTH * tetris::VISIBLE_HEIGHT> board{};
    /// Four movable cells per preview slot, enough to draw any piece.
    std::array<std::array<Entity*, 4>, tetris::NEXT_QUEUE_SIZE> next{};
    std::array<Entity*, 4> hold{};
    SevenSegmentDisplay score;
    SevenSegmentDisplay lines;
    SevenSegmentDisplay level;
};

/// Plays a game of Tetris: reads the keyboard, runs the rules in
/// tetris::Game, and paints the result onto the scene's entities.
///
/// Rows that clear, and the whole stack when the game ends, break up into
/// Box2D bodies that tumble off the board.
class TetrisController : public Component {
public:
    TetrisController(Entity& owner, TetrisViews views);

    void update(GLFWwindow* window, float deltaTime) override;

private:
    /// Edge detection over a set of keys that all do the same thing.
    struct KeyBinding {
        std::vector<int> keys;
        bool held = false;
        bool pressed = false;

        KeyBinding(std::initializer_list<int> codes) : keys(codes) {}
        void poll(GLFWwindow* window);
    };

    void handleInput(GLFWwindow* window, float deltaTime);
    /// Auto-repeat for sideways movement: one step on press, then after
    /// DAS_DELAY one step every DAS_REPEAT for as long as the key is held.
    void handleSidewaysMovement(float deltaTime);
    void refreshBoard();
    void refreshPreview(const std::array<Entity*, 4>& cells, b2Vec2 centre,
                        std::optional<tetris::PieceType> type, bool dimmed);
    void refreshReadouts(GLFWwindow* window);
    void spawnDebris(const std::vector<tetris::ClearedCell>& cells);
    void shatterStack();

    tetris::Game m_Game;
    TetrisViews m_Views;

    KeyBinding m_Left{GLFW_KEY_LEFT, GLFW_KEY_A};
    KeyBinding m_Right{GLFW_KEY_RIGHT, GLFW_KEY_D};
    KeyBinding m_SoftDrop{GLFW_KEY_DOWN, GLFW_KEY_S};
    KeyBinding m_HardDrop{GLFW_KEY_SPACE};
    KeyBinding m_RotateClockwise{GLFW_KEY_UP, GLFW_KEY_X, GLFW_KEY_W};
    KeyBinding m_RotateCounterClockwise{GLFW_KEY_Z, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL};
    KeyBinding m_Hold{GLFW_KEY_C, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT};
    KeyBinding m_Pause{GLFW_KEY_P};
    KeyBinding m_Restart{GLFW_KEY_R};

    /// Direction auto-repeat is moving in: -1 left, 1 right, 0 none.
    int m_RepeatDirection = 0;
    float m_RepeatTimer = 0.f;

    bool m_WasGameOver = false;
    std::string m_Title;
};

#endif //MAPLETETRIS_TETRISCONTROLLER_H
