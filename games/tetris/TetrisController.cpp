#include "TetrisController.h"
#include "TetrisLayout.h"
#include "World.h"
#include "ECS/Components/BehaviourScript.h"
#include "Scripts/ExpireAfter.h"
#include "Audio/Audio.h"
#include <algorithm>
#include <cstdio>
#include <random>

using namespace tetris;

namespace {

/// Delayed auto shift: how long a sideways key is held before it repeats,
/// and how often it repeats after that.
constexpr float DAS_DELAY = 0.17f;
constexpr float DAS_REPEAT = 0.05f;
/// Longest frame the rules are advanced by in one go, so a stall (a window
/// drag, a breakpoint) cannot drop a piece halfway down the well.
constexpr float MAX_FRAME_TIME = 0.1f;
/// Seconds debris stays alive. By then it has fallen off the screen.
constexpr float DEBRIS_LIFETIME = 2.5f;

constexpr float MUSIC_VOLUME = 0.6f;
/// The music speeds up a little with every level, up to this rate.
constexpr float MUSIC_PITCH_STEP = 0.02f;
constexpr float MAX_MUSIC_PITCH = 1.3f;

glm::vec3 mix(const glm::vec3& from, const glm::vec3& to, float amount)
{
    return from + (to - from) * amount;
}

std::mt19937& debrisRandom()
{
    static std::mt19937 random{std::random_device{}()};
    return random;
}

float randomBetween(float low, float high)
{
    return std::uniform_real_distribution<float>(low, high)(debrisRandom());
}

/// A dynamic body that leaves the board with a kick and a spin. Its shape is
/// a sensor so the pieces tumble through each other and through the frame
/// instead of piling up at the bottom of the well.
void spawnDebrisCell(b2Vec2 centre, glm::vec3 colour)
{
    World& game = World::getInstance();
    const float half = layout::CELL_SIZE / 2.f;

    PhysicsMaterial material;
    material.isSensor = true;

    auto debris = new Entity(game.getPhysics().getWorld(),
                             b2Vec2{centre.x - half, centre.y + half},
                             b2Vec2{centre.x + half, centre.y - half},
                             BodyType::Dynamic, material);

    b2Body_SetLinearVelocity(debris->body, b2Vec2{randomBetween(-6.f, 6.f), randomBetween(3.f, 12.f)});
    b2Body_SetAngularVelocity(debris->body, randomBetween(-12.f, 12.f));

    auto renderer = new BoxRenderer(*debris, layout::CELL_SIZE, layout::CELL_SIZE, 1.f);
    renderer->setColour(colour);
    debris->addComponent(renderer);
    debris->addComponent(new BehaviourScript(*debris, expireAfter(DEBRIS_LIFETIME)));

    game.addEntity(debris);
}

/// Move a preview cell and show it. Preview cells are static bodies, so
/// teleporting them is fine: nothing is simulated against them.
void placeCell(Entity* cell, b2Vec2 position, glm::vec3 colour)
{
    b2Body_SetTransform(cell->body, position, b2Rot_identity);

    auto renderer = cell->getComponent<BoxRenderer>();
    renderer->setColour(colour);
    renderer->setVisible(true);
}

} // namespace

void TetrisController::KeyBinding::poll(GLFWwindow* window)
{
    const bool down = std::any_of(keys.begin(), keys.end(),
                                  [window](int key) { return glfwGetKey(window, key) == GLFW_PRESS; });
    pressed = down && !held;
    held = down;
}

TetrisController::TetrisController(Entity& owner, TetrisViews views)
    : Component(owner), m_Views(std::move(views)), m_Sounds(createTetrisSounds())
{}

void TetrisController::update(GLFWwindow* window, float deltaTime)
{
    deltaTime = std::min(deltaTime, MAX_FRAME_TIME);

    // Started from the first frame rather than the constructor, so the music
    // does not run ahead while the scene is still being built.
    if (!m_MusicStarted)
    {
        startMusic();
        m_MusicStarted = true;
    }

    const int linesBefore = m_Game.lines();
    const int levelBefore = m_Game.level();
    const int lockedBefore = m_Game.piecesLocked();
    m_HardDropped = false;

    handleInput(window, deltaTime);
    m_Game.update(deltaTime, m_SoftDrop.held);

    spawnDebris(m_Game.takeClearedCells());
    playEventSounds(linesBefore, levelBefore, lockedBefore);

    if (m_Game.isGameOver() && !m_WasGameOver)
        shatterStack();
    m_WasGameOver = m_Game.isGameOver();

    refreshBoard();

    for (int i = 0; i < NEXT_QUEUE_SIZE; ++i)
        refreshPreview(m_Views.next[i], layout::NEXT_SLOT_CENTRES[i], m_Game.nextPieces()[i], false);
    refreshPreview(m_Views.hold, layout::HOLD_PANEL_CENTRE, m_Game.heldPiece(), !m_Game.canHold());

    refreshReadouts(window);
}

void TetrisController::handleInput(GLFWwindow* window, float deltaTime)
{
    for (KeyBinding* binding: {&m_Left, &m_Right, &m_SoftDrop, &m_HardDrop, &m_RotateClockwise,
                               &m_RotateCounterClockwise, &m_Hold, &m_Pause, &m_Restart,
                               &m_ToggleMusic})
        binding->poll(window);

    if (m_ToggleMusic.pressed)
    {
        m_MusicEnabled = !m_MusicEnabled;

        if (m_MusicEnabled && !m_Game.isGameOver())
            startMusic();
        else
            Audio::getInstance().stopMusic();
    }

    if (m_Restart.pressed)
    {
        m_Game.reset();
        m_RepeatDirection = 0;
        startMusic();
        return;
    }

    if (m_Pause.pressed && !m_Game.isGameOver())
    {
        m_Game.setPaused(!m_Game.isPaused());
        Audio::getInstance().setMusicPaused(m_Game.isPaused());
        playSound(m_Sounds.pause);
    }

    if (m_Game.isPaused() || m_Game.isGameOver())
        return;

    // Rotate and hold before moving, so a rotation pressed on the same frame
    // as a slide is checked against the piece's current position.
    if (m_RotateClockwise.pressed && m_Game.rotateClockwise())
        playSound(m_Sounds.rotate);
    if (m_RotateCounterClockwise.pressed && m_Game.rotateCounterClockwise())
        playSound(m_Sounds.rotate);
    if (m_Hold.pressed && m_Game.hold())
        playSound(m_Sounds.hold);

    handleSidewaysMovement(deltaTime);

    if (m_HardDrop.pressed)
    {
        m_Game.hardDrop();
        m_HardDropped = true;
        playSound(m_Sounds.hardDrop);
    }
}

void TetrisController::handleSidewaysMovement(float deltaTime)
{
    auto step = [this](int direction) {
        const bool moved = direction < 0 ? m_Game.moveLeft() : m_Game.moveRight();
        if (moved)
            playSound(m_Sounds.move);
        return moved;
    };

    // The most recently pressed direction wins, as on most modern clients.
    if (m_Left.pressed || m_Right.pressed)
    {
        m_RepeatDirection = m_Left.pressed ? -1 : 1;
        m_RepeatTimer = DAS_DELAY;
        step(m_RepeatDirection);
        return;
    }

    // Releasing the active direction hands over to the other key if it is
    // still down, starting its delay from scratch.
    const bool activeHeld = m_RepeatDirection < 0 ? m_Left.held : m_Right.held;

    if (m_RepeatDirection != 0 && !activeHeld)
    {
        const bool otherHeld = m_RepeatDirection < 0 ? m_Right.held : m_Left.held;
        m_RepeatDirection = otherHeld ? -m_RepeatDirection : 0;
        m_RepeatTimer = DAS_DELAY;
    }

    if (m_RepeatDirection == 0)
        return;

    m_RepeatTimer -= deltaTime;

    while (m_RepeatTimer <= 0.f)
    {
        m_RepeatTimer += DAS_REPEAT;

        if (!step(m_RepeatDirection))
        {
            // Against a wall: keep charged, so the piece slides the moment
            // the way is clear.
            m_RepeatTimer = 0.f;
            break;
        }
    }
}

void TetrisController::refreshBoard()
{
    // While paused the stack and the piece are hidden, so pausing cannot be
    // used to plan ahead. After a game over the stack has turned to debris.
    const bool showStack = !m_Game.isPaused() && !m_Game.isGameOver();

    for (int y = 0; y < VISIBLE_HEIGHT; ++y)
        for (int x = 0; x < BOARD_WIDTH; ++x)
        {
            const auto cell = showStack ? m_Game.cellAt(x, y) : std::nullopt;
            m_Views.board[y * BOARD_WIDTH + x]->setColour(
                    cell ? layout::pieceColour(*cell) : layout::EMPTY_CELL_COLOUR);
        }

    if (!showStack)
        return;

    const Piece active = m_Game.activePiece();
    const glm::vec3 colour = layout::pieceColour(active.type);
    const glm::vec3 ghostColour = mix(layout::EMPTY_CELL_COLOUR, colour, layout::GHOST_STRENGTH);

    // Ghost first, so the piece draws over it where they overlap.
    for (const Point& cell: m_Game.ghostPiece().cells())
        if (cell.y < VISIBLE_HEIGHT)
            m_Views.board[cell.y * BOARD_WIDTH + cell.x]->setColour(ghostColour);

    for (const Point& cell: active.cells())
        if (cell.y < VISIBLE_HEIGHT)
            m_Views.board[cell.y * BOARD_WIDTH + cell.x]->setColour(colour);
}

void TetrisController::refreshPreview(const std::array<Entity*, 4>& cells, b2Vec2 centre,
                                      std::optional<PieceType> type, bool dimmed)
{
    if (!type || m_Game.isPaused())
    {
        for (Entity* cell: cells)
            cell->getComponent<BoxRenderer>()->setVisible(false);
        return;
    }

    const PieceCells shape = pieceShape(*type, 0);
    float minX = 4.f, maxX = 0.f, minY = 4.f, maxY = 0.f;

    for (const Point& point: shape)
    {
        minX = std::min(minX, (float)point.x);
        maxX = std::max(maxX, (float)point.x);
        minY = std::min(minY, (float)point.y);
        maxY = std::max(maxY, (float)point.y);
    }

    // Centre the piece's own bounding box on the slot, so a three wide piece
    // sits in the middle instead of half a cell off.
    const float offsetX = (minX + maxX) / 2.f;
    const float offsetY = (minY + maxY) / 2.f;
    glm::vec3 colour = layout::pieceColour(*type);

    if (dimmed)
        colour = mix(layout::PANEL_COLOUR, colour, 0.35f);

    for (int i = 0; i < 4; ++i)
    {
        const b2Vec2 position{centre.x + ((float)shape[i].x - offsetX) * layout::PREVIEW_SCALE,
                              centre.y + ((float)shape[i].y - offsetY) * layout::PREVIEW_SCALE};
        placeCell(cells[i], position, colour);
    }
}

void TetrisController::refreshReadouts(GLFWwindow* window)
{
    m_Views.score.setValue(m_Game.score());
    m_Views.lines.setValue((std::uint32_t)m_Game.lines());
    m_Views.level.setValue((std::uint32_t)m_Game.level());

    char title[160];
    const char* status = m_Game.isGameOver() ? "  -  GAME OVER, press R to play again"
                       : m_Game.isPaused()   ? "  -  PAUSED, press P to resume"
                                             : "";
    snprintf(title, sizeof(title), "Maple Tetris  -  Score %u  |  Lines %d  |  Level %d%s",
             m_Game.score(), m_Game.lines(), m_Game.level(), status);

    // Setting the title is a round trip to the window system, so only do it
    // when the text changes.
    if (m_Title != title)
    {
        m_Title = title;
        glfwSetWindowTitle(window, title);
    }
}

void TetrisController::spawnDebris(const std::vector<ClearedCell>& cells)
{
    for (const ClearedCell& cell: cells)
        if (cell.position.y < VISIBLE_HEIGHT)
            spawnDebrisCell(layout::cellCentre(cell.position.x, cell.position.y),
                            layout::pieceColour(cell.type));
}

void TetrisController::shatterStack()
{
    for (int y = 0; y < VISIBLE_HEIGHT; ++y)
        for (int x = 0; x < BOARD_WIDTH; ++x)
            if (const auto cell = m_Game.cellAt(x, y))
                spawnDebrisCell(layout::cellCentre(x, y), layout::pieceColour(*cell));
}

void TetrisController::playSound(const SoundClipPtr& clip, float volume)
{
    Audio::getInstance().play(clip, volume);
}

void TetrisController::startMusic()
{
    Audio& audio = Audio::getInstance();

    if (!m_MusicEnabled)
        return;

    audio.playMusic(m_Sounds.music, MUSIC_VOLUME);
    audio.setMusicPitch(1.f);
}

void TetrisController::playEventSounds(int linesBefore, int levelBefore, int lockedBefore)
{
    const int cleared = m_Game.lines() - linesBefore;

    if (cleared > 0)
        playSound(m_Sounds.lineClear[std::min(cleared, 4) - 1]);
    else if (m_Game.piecesLocked() > lockedBefore && !m_HardDropped)
        playSound(m_Sounds.lock);

    if (m_Game.level() > levelBefore)
    {
        playSound(m_Sounds.levelUp);
        Audio::getInstance().setMusicPitch(
                std::min(1.f + MUSIC_PITCH_STEP * (float)(m_Game.level() - 1), MAX_MUSIC_PITCH));
    }

    if (m_Game.isGameOver() && !m_WasGameOver)
    {
        Audio::getInstance().stopMusic();
        playSound(m_Sounds.gameOver);
    }
}
