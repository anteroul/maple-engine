#ifndef MAPLETETRIS_TETRISLAYOUT_H
#define MAPLETETRIS_TETRISLAYOUT_H

#include "TetrisGame.h"
#include <box2d/box2d.h>
#include <glm/glm.hpp>

/// Where everything sits in world space. One unit is one board cell, and the
/// board's bottom left corner is the origin, so cell (x, y) is centred on
/// (x + 0.5, y + 0.5).
namespace tetris::layout {

/// Region of world space the camera shows: the board, a margin, and the side
/// panel on the right.
constexpr float VIEW_LEFT = -1.f;
constexpr float VIEW_RIGHT = 17.f;
constexpr float VIEW_BOTTOM = -1.f;
constexpr float VIEW_TOP = 21.f;

/// Drawn size of a cell, a little under 1 so neighbouring cells show a gap.
constexpr float CELL_SIZE = 0.92f;
/// Cells in the side panel previews are drawn smaller than board cells.
constexpr float PREVIEW_SCALE = 0.8f;

constexpr float PANEL_CENTRE_X = 13.5f;
constexpr float PANEL_WIDTH = 5.f;

/// Next queue panel, and the centre of each of its preview slots, nearest
/// piece first.
constexpr b2Vec2 NEXT_PANEL_CENTRE{PANEL_CENTRE_X, 15.75f};
constexpr b2Vec2 NEXT_PANEL_SIZE{PANEL_WIDTH, 8.5f};
constexpr b2Vec2 NEXT_SLOT_CENTRES[NEXT_QUEUE_SIZE] = {
        {PANEL_CENTRE_X, 18.5f}, {PANEL_CENTRE_X, 15.75f}, {PANEL_CENTRE_X, 13.f}};

constexpr b2Vec2 HOLD_PANEL_CENTRE{PANEL_CENTRE_X, 8.75f};
constexpr b2Vec2 HOLD_PANEL_SIZE{PANEL_WIDTH, 3.5f};

constexpr b2Vec2 SCORE_PANEL_CENTRE{PANEL_CENTRE_X, 3.f};
constexpr b2Vec2 SCORE_PANEL_SIZE{PANEL_WIDTH, 6.f};

/// Seven segment readouts: bottom edge of each row of digits.
constexpr float SCORE_DIGITS_Y = 4.2f;
constexpr float LINES_DIGITS_Y = 2.4f;
constexpr float LEVEL_DIGITS_Y = 0.6f;
constexpr int SCORE_DIGITS = 6;
constexpr int LINES_DIGITS = 3;
constexpr int LEVEL_DIGITS = 2;
constexpr float DIGIT_WIDTH = 0.6f;
constexpr float DIGIT_HEIGHT = 1.2f;
constexpr float DIGIT_GAP = 0.15f;
constexpr float SEGMENT_THICKNESS = 0.14f;

inline b2Vec2 cellCentre(int x, int y)
{
    return b2Vec2{(float)x + 0.5f, (float)y + 0.5f};
}

/// Guideline colours.
inline glm::vec3 pieceColour(PieceType type)
{
    switch (type)
    {
        case PieceType::I: return {0.f, 0.85f, 0.95f};
        case PieceType::O: return {0.98f, 0.85f, 0.f};
        case PieceType::T: return {0.65f, 0.2f, 0.9f};
        case PieceType::S: return {0.2f, 0.85f, 0.25f};
        case PieceType::Z: return {0.95f, 0.15f, 0.2f};
        case PieceType::J: return {0.15f, 0.35f, 0.95f};
        case PieceType::L: return {0.98f, 0.55f, 0.05f};
    }
    return {1.f, 1.f, 1.f};
}

const glm::vec3 BACKGROUND_COLOUR{0.04f, 0.04f, 0.07f};
const glm::vec3 FRAME_COLOUR{0.35f, 0.36f, 0.45f};
const glm::vec3 WELL_COLOUR{0.02f, 0.02f, 0.03f};
const glm::vec3 EMPTY_CELL_COLOUR{0.07f, 0.07f, 0.1f};
const glm::vec3 PANEL_COLOUR{0.08f, 0.08f, 0.12f};
/// How strongly the ghost piece shows its piece's colour over an empty cell.
constexpr float GHOST_STRENGTH = 0.3f;

const glm::vec3 SCORE_COLOUR{0.95f, 0.95f, 0.95f};
const glm::vec3 LINES_COLOUR{0.f, 0.85f, 0.95f};
const glm::vec3 LEVEL_COLOUR{0.98f, 0.55f, 0.05f};
/// An unlit segment is still faintly drawn, like a real display.
constexpr float UNLIT_SEGMENT_STRENGTH = 0.12f;

} // namespace tetris::layout

#endif //MAPLETETRIS_TETRISLAYOUT_H
