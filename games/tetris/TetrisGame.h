#ifndef MAPLETETRIS_TETRISGAME_H
#define MAPLETETRIS_TETRISGAME_H

#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

/// Tetris rules, with no rendering, input or engine dependencies, so they can
/// be tested headless. Follows the modern guideline: SRS rotation with wall
/// kicks, a 7-bag randomiser, hold, a lock delay with move reset, and
/// guideline scoring and gravity.
///
/// Coordinates: x is the column, 0 on the left; y is the row, 0 at the
/// bottom, growing upwards to match the engine's world space.
namespace tetris {

constexpr int BOARD_WIDTH = 10;
/// Rows the player sees. Pieces spawn in its top two rows.
constexpr int VISIBLE_HEIGHT = 20;
/// Rows the board actually has. The hidden rows above the visible ones give
/// rotations and kicks near the top somewhere to go.
constexpr int BOARD_HEIGHT = 24;
constexpr int NEXT_QUEUE_SIZE = 3;

/// Seconds a piece may rest on the stack before it locks.
constexpr float LOCK_DELAY = 0.5f;
/// Moves or rotations that may restart the lock delay before the piece locks
/// regardless. Stops a player from stalling forever by spinning in place.
constexpr int MAX_LOCK_RESETS = 15;
/// Soft drop falls this many times faster than gravity.
constexpr float SOFT_DROP_FACTOR = 20.f;

enum class PieceType : std::uint8_t { I, O, T, S, Z, J, L };
constexpr int PIECE_TYPE_COUNT = 7;

struct Point {
    int x, y;
    bool operator==(const Point&) const = default;
};

using PieceCells = std::array<Point, 4>;

/// Cells of `type` in rotation state `rotation` (0 spawn, 1 right, 2 reverse,
/// 3 left), relative to the bottom left of the piece's bounding box.
PieceCells pieceShape(PieceType type, int rotation);

struct Piece {
    PieceType type;
    int rotation;
    /// Bottom left of the piece's bounding box on the board.
    Point origin;

    PieceCells cells() const;
};

struct ClearedCell {
    Point position;
    PieceType type;
};

class Game {
public:
    explicit Game(std::uint32_t seed = std::random_device{}());

    /// Start over: empty board, fresh bag, score back to zero.
    void reset();

    /// Player actions. Each returns whether it did anything.
    bool moveLeft();
    bool moveRight();
    bool rotateClockwise();
    bool rotateCounterClockwise();
    /// Swap the active piece with the held one. Once per piece.
    bool hold();
    /// Drop the piece straight down and lock it.
    /// \return Rows the piece fell.
    int hardDrop();

    /// Advance gravity and the lock delay.
    /// \param deltaTime Seconds since the last update.
    /// \param softDrop Whether the player is holding soft drop.
    void update(float deltaTime, bool softDrop);

    void setPaused(bool paused) { m_Paused = paused; }
    bool isPaused() const { return m_Paused; }
    bool isGameOver() const { return m_GameOver; }

    /// The locked cell at (x, y), if any. Out of bounds reads as empty.
    std::optional<PieceType> cellAt(int x, int y) const;
    const Piece& activePiece() const { return m_Active; }
    /// Where the active piece would land if hard dropped.
    Piece ghostPiece() const;
    std::array<PieceType, NEXT_QUEUE_SIZE> nextPieces() const;
    std::optional<PieceType> heldPiece() const { return m_Held; }
    bool canHold() const { return m_CanHold; }

    std::uint32_t score() const { return m_Score; }
    int lines() const { return m_Lines; }
    int level() const { return 1 + m_Lines / 10; }
    /// Seconds per row of gravity at the current level.
    float gravityInterval() const;

    /// Cells removed by line clears since the last call, so a front end can
    /// animate them. Clears the list.
    std::vector<ClearedCell> takeClearedCells();

    /// Testing hooks: set up a board and a piece directly.
    void setCell(int x, int y, std::optional<PieceType> type);
    void setActivePiece(PieceType type);

private:
    bool fits(const Piece& piece) const;
    bool tryMove(int dx, int dy);
    bool tryRotate(int direction);
    bool isGrounded() const;
    /// A successful move or rotation restarts the lock delay, a limited
    /// number of times per piece.
    void onPieceMoved();
    void lockPiece();
    void clearLines();
    /// Make `type` the active piece at the spawn position. Ends the game if
    /// it does not fit there.
    void spawn(PieceType type);
    void spawnNext();
    PieceType drawFromBag();
    void refillBag();

    std::mt19937 m_Random;
    std::array<std::array<std::optional<PieceType>, BOARD_WIDTH>, BOARD_HEIGHT> m_Board{};
    /// Upcoming pieces, nearest first. Always holds at least NEXT_QUEUE_SIZE.
    std::vector<PieceType> m_Queue;
    Piece m_Active{PieceType::T, 0, {0, 0}};
    std::optional<PieceType> m_Held;
    bool m_CanHold = true;

    float m_FallTimer = 0.f;
    float m_LockTimer = 0.f;
    int m_LockResets = 0;
    /// Lowest row the active piece has reached. Reaching a new low gives the
    /// player back their lock resets.
    int m_LowestY = 0;

    std::uint32_t m_Score = 0;
    int m_Lines = 0;
    bool m_GameOver = false;
    bool m_Paused = false;
    std::vector<ClearedCell> m_ClearedCells;
};

} // namespace tetris

#endif //MAPLETETRIS_TETRISGAME_H
