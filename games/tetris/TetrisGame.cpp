#include "TetrisGame.h"
#include <algorithm>
#include <cmath>

namespace tetris {

namespace {

/// Side of each piece's bounding box. Rotation turns the piece inside it.
int boxSize(PieceType type)
{
    switch (type)
    {
        case PieceType::I: return 4;
        case PieceType::O: return 2;
        default:           return 3;
    }
}

/// Spawn orientation of every piece, as SRS defines it.
PieceCells spawnShape(PieceType type)
{
    switch (type)
    {
        case PieceType::I: return {{{0, 2}, {1, 2}, {2, 2}, {3, 2}}};
        case PieceType::O: return {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
        case PieceType::T: return {{{0, 1}, {1, 1}, {2, 1}, {1, 2}}};
        case PieceType::S: return {{{0, 1}, {1, 1}, {1, 2}, {2, 2}}};
        case PieceType::Z: return {{{0, 2}, {1, 2}, {1, 1}, {2, 1}}};
        case PieceType::J: return {{{0, 2}, {0, 1}, {1, 1}, {2, 1}}};
        case PieceType::L: return {{{2, 2}, {0, 1}, {1, 1}, {2, 1}}};
    }
    return {};
}

/// Wall kicks. When a rotation does not fit, SRS tries these offsets in
/// order and keeps the first that does. Indexed by [from state][direction],
/// direction 0 clockwise and 1 counter-clockwise; y points up.
using KickTable = std::array<std::array<std::array<Point, 5>, 2>, 4>;

constexpr KickTable JLSTZ_KICKS = {{
    {{ {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}},     // 0 -> R
       {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}} }},     // 0 -> L
    {{ {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},         // R -> 2
       {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}} }},      // R -> 0
    {{ {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}},        // 2 -> L
       {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}} }},  // 2 -> R
    {{ {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}},      // L -> 0
       {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}} }},   // L -> 2
}};

constexpr KickTable I_KICKS = {{
    {{ {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}},       // 0 -> R
       {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}} }},    // 0 -> L
    {{ {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}},       // R -> 2
       {{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}} }},    // R -> 0
    {{ {{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}},       // 2 -> L
       {{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}} }},    // 2 -> R
    {{ {{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}},       // L -> 0
       {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}} }},    // L -> 2
}};

/// Bounding box position that puts a fresh piece in the top two visible
/// rows, centred, leaning left when it cannot be exactly centred.
Point spawnOrigin(PieceType type)
{
    // The I and JLSTZ shapes sit in the second row from the top of their
    // box; the O fills its whole box.
    if (type == PieceType::O)
        return {4, VISIBLE_HEIGHT - 2};
    return {3, VISIBLE_HEIGHT - 3};
}

/// Points for clearing 1, 2, 3 or 4 lines at once, before the level bonus.
constexpr std::array<std::uint32_t, 5> LINE_CLEAR_POINTS = {0, 100, 300, 500, 800};

} // namespace

PieceCells pieceShape(PieceType type, int rotation)
{
    const int size = boxSize(type);
    PieceCells cells = spawnShape(type);
    rotation = ((rotation % 4) + 4) % 4;

    // A quarter turn clockwise inside the box: (x, y) -> (y, size - 1 - x).
    for (int turn = 0; turn < rotation; ++turn)
        for (Point& cell: cells)
            cell = {cell.y, size - 1 - cell.x};

    return cells;
}

PieceCells Piece::cells() const
{
    PieceCells result = pieceShape(type, rotation);

    for (Point& cell: result)
    {
        cell.x += origin.x;
        cell.y += origin.y;
    }

    return result;
}

Game::Game(std::uint32_t seed) : m_Random(seed)
{
    reset();
}

void Game::reset()
{
    for (auto& row: m_Board)
        row.fill(std::nullopt);

    m_Queue.clear();
    m_Held.reset();
    m_Score = 0;
    m_Lines = 0;
    m_PiecesLocked = 0;
    m_GameOver = false;
    m_Paused = false;
    m_ClearedCells.clear();

    spawnNext();
}

bool Game::fits(const Piece& piece) const
{
    for (const Point& cell: piece.cells())
    {
        if (cell.x < 0 || cell.x >= BOARD_WIDTH || cell.y < 0 || cell.y >= BOARD_HEIGHT)
            return false;
        if (m_Board[cell.y][cell.x])
            return false;
    }
    return true;
}

bool Game::isGrounded() const
{
    Piece below = m_Active;
    below.origin.y -= 1;
    return !fits(below);
}

void Game::onPieceMoved()
{
    if (m_Active.origin.y < m_LowestY)
    {
        m_LowestY = m_Active.origin.y;
        m_LockResets = 0;
    }

    if (isGrounded() && m_LockResets < MAX_LOCK_RESETS)
    {
        m_LockTimer = 0.f;
        m_LockResets++;
    }
}

bool Game::tryMove(int dx, int dy)
{
    if (m_GameOver || m_Paused)
        return false;

    Piece moved = m_Active;
    moved.origin.x += dx;
    moved.origin.y += dy;

    if (!fits(moved))
        return false;

    m_Active = moved;
    onPieceMoved();
    return true;
}

bool Game::tryRotate(int direction)
{
    if (m_GameOver || m_Paused)
        return false;

    // The O looks the same in every orientation; turning it would only move
    // it around its box.
    if (m_Active.type == PieceType::O)
        return false;

    const KickTable& kicks = m_Active.type == PieceType::I ? I_KICKS : JLSTZ_KICKS;
    const int from = m_Active.rotation;
    const int to = (from + (direction > 0 ? 1 : 3)) % 4;

    for (const Point& kick: kicks[from][direction > 0 ? 0 : 1])
    {
        Piece rotated = m_Active;
        rotated.rotation = to;
        rotated.origin.x += kick.x;
        rotated.origin.y += kick.y;

        if (fits(rotated))
        {
            m_Active = rotated;
            onPieceMoved();
            return true;
        }
    }

    return false;
}

bool Game::moveLeft() { return tryMove(-1, 0); }
bool Game::moveRight() { return tryMove(1, 0); }
bool Game::rotateClockwise() { return tryRotate(1); }
bool Game::rotateCounterClockwise() { return tryRotate(-1); }

bool Game::hold()
{
    if (m_GameOver || m_Paused || !m_CanHold)
        return false;

    const PieceType current = m_Active.type;

    if (m_Held)
        spawn(*m_Held);
    else
        spawnNext();

    m_Held = current;
    m_CanHold = false;
    return true;
}

int Game::hardDrop()
{
    if (m_GameOver || m_Paused)
        return 0;

    int rows = 0;
    Piece below = m_Active;
    below.origin.y -= 1;

    while (fits(below))
    {
        m_Active = below;
        below.origin.y -= 1;
        rows++;
    }

    m_Score += 2 * rows;
    lockPiece();
    return rows;
}

float Game::gravityInterval() const
{
    // The guideline curve: one row per second at level 1, speeding up until
    // pieces drop a row every frame or faster.
    const int capped = std::min(level(), 20);
    return std::pow(0.8f - (float)(capped - 1) * 0.007f, (float)(capped - 1));
}

void Game::update(float deltaTime, bool softDrop)
{
    if (m_GameOver || m_Paused || deltaTime <= 0.f)
        return;

    float interval = gravityInterval();

    if (softDrop)
        interval /= SOFT_DROP_FACTOR;

    if (isGrounded())
    {
        // Gravity has nowhere to go, so it must not bank time for later: the
        // moment the piece slid off a ledge, it would fall several rows at
        // once.
        m_FallTimer = 0.f;
        m_LockTimer += deltaTime;

        if (m_LockTimer >= LOCK_DELAY)
            lockPiece();
        return;
    }

    m_LockTimer = 0.f;
    m_FallTimer += deltaTime;

    while (m_FallTimer >= interval)
    {
        m_FallTimer -= interval;

        Piece below = m_Active;
        below.origin.y -= 1;

        if (!fits(below))
        {
            m_FallTimer = 0.f;
            break;
        }

        m_Active = below;

        if (softDrop)
            m_Score += 1;

        if (m_Active.origin.y < m_LowestY)
        {
            m_LowestY = m_Active.origin.y;
            m_LockResets = 0;
        }
    }
}

void Game::lockPiece()
{
    bool anyCellVisible = false;

    for (const Point& cell: m_Active.cells())
    {
        m_Board[cell.y][cell.x] = m_Active.type;

        if (cell.y < VISIBLE_HEIGHT)
            anyCellVisible = true;
    }

    m_PiecesLocked++;
    clearLines();

    // Lock out: the piece came to rest entirely above the visible board.
    if (!anyCellVisible)
    {
        m_GameOver = true;
        return;
    }

    m_CanHold = true;
    spawnNext();
}

void Game::clearLines()
{
    int cleared = 0;
    int writeRow = 0;

    for (int y = 0; y < BOARD_HEIGHT; ++y)
    {
        const bool full = std::all_of(m_Board[y].begin(), m_Board[y].end(),
                                      [](const auto& cell) { return cell.has_value(); });

        if (full)
        {
            for (int x = 0; x < BOARD_WIDTH; ++x)
                m_ClearedCells.push_back({{x, y}, *m_Board[y][x]});
            cleared++;
            continue;
        }

        if (writeRow != y)
            m_Board[writeRow] = m_Board[y];
        writeRow++;
    }

    for (int y = writeRow; y < BOARD_HEIGHT; ++y)
        m_Board[y].fill(std::nullopt);

    if (cleared == 0)
        return;

    // Score at the level the lines were cleared on, before they count
    // towards the next one.
    m_Score += LINE_CLEAR_POINTS[cleared] * (std::uint32_t)level();
    m_Lines += cleared;
}

void Game::spawn(PieceType type)
{
    m_Active = Piece{type, 0, spawnOrigin(type)};
    m_FallTimer = 0.f;
    m_LockTimer = 0.f;
    m_LockResets = 0;
    m_LowestY = m_Active.origin.y;

    // Block out: the new piece overlaps the stack.
    if (!fits(m_Active))
        m_GameOver = true;
}

void Game::spawnNext()
{
    spawn(drawFromBag());
}

PieceType Game::drawFromBag()
{
    while ((int)m_Queue.size() <= NEXT_QUEUE_SIZE)
        refillBag();

    const PieceType type = m_Queue.front();
    m_Queue.erase(m_Queue.begin());
    return type;
}

void Game::refillBag()
{
    // One of each piece, shuffled. Guarantees no piece is ever more than 12
    // draws away, unlike drawing each piece independently.
    std::array<PieceType, PIECE_TYPE_COUNT> bag{};

    for (int i = 0; i < PIECE_TYPE_COUNT; ++i)
        bag[i] = (PieceType)i;

    std::shuffle(bag.begin(), bag.end(), m_Random);
    m_Queue.insert(m_Queue.end(), bag.begin(), bag.end());
}

std::optional<PieceType> Game::cellAt(int x, int y) const
{
    if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT)
        return std::nullopt;
    return m_Board[y][x];
}

Piece Game::ghostPiece() const
{
    Piece ghost = m_Active;
    Piece below = ghost;
    below.origin.y -= 1;

    while (fits(below))
    {
        ghost = below;
        below.origin.y -= 1;
    }

    return ghost;
}

std::array<PieceType, NEXT_QUEUE_SIZE> Game::nextPieces() const
{
    std::array<PieceType, NEXT_QUEUE_SIZE> next{};

    for (int i = 0; i < NEXT_QUEUE_SIZE; ++i)
        next[i] = m_Queue[i];

    return next;
}

std::vector<ClearedCell> Game::takeClearedCells()
{
    std::vector<ClearedCell> cells;
    cells.swap(m_ClearedCells);
    return cells;
}

void Game::setCell(int x, int y, std::optional<PieceType> type)
{
    if (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT)
        m_Board[y][x] = type;
}

void Game::setActivePiece(PieceType type)
{
    spawn(type);
}

} // namespace tetris
