#include "TetrisScene.h"
#include "TetrisController.h"
#include "TetrisLayout.h"
#include "ECS/Components/BoxRenderer.h"

using namespace tetris;

namespace {

/// Gravity for debris, in cells per second squared. Earth gravity would be
/// 9.81 cells, which at this scale looks like falling through syrup.
constexpr b2Vec2 DEBRIS_GRAVITY{0.f, -40.f};

/// A static, sensor-only box with a renderer. Everything in the scene is
/// drawn with these: the board, its frame and the side panel. As sensors they
/// never collide with each other or with the debris.
Entity* addBox(World& game, b2Vec2 centre, b2Vec2 size, glm::vec3 colour)
{
    PhysicsMaterial material;
    material.isSensor = true;

    auto entity = new Entity(game.getPhysics().getWorld(),
                             b2Vec2{centre.x - size.x / 2.f, centre.y + size.y / 2.f},
                             b2Vec2{centre.x + size.x / 2.f, centre.y - size.y / 2.f},
                             BodyType::Static, material);

    auto renderer = new BoxRenderer(*entity, size.x, size.y, 1.f);
    renderer->setColour(colour);
    entity->addComponent(renderer);

    game.addEntity(entity);
    return entity;
}

BoxRenderer* addBoxRenderer(World& game, b2Vec2 centre, b2Vec2 size, glm::vec3 colour)
{
    return addBox(game, centre, size, colour)->getComponent<BoxRenderer>();
}

/// A panel: a frame with a darker inside.
void addPanel(World& game, b2Vec2 centre, b2Vec2 size)
{
    constexpr float BORDER = 0.15f;
    addBox(game, centre, b2Vec2{size.x + 2.f * BORDER, size.y + 2.f * BORDER}, layout::FRAME_COLOUR);
    addBox(game, centre, size, layout::PANEL_COLOUR);
}

SevenSegmentDisplay addReadout(World& game, int digitCount, float bottom, glm::vec3 colour)
{
    using namespace layout;

    const float width = (float)digitCount * DIGIT_WIDTH + (float)(digitCount - 1) * DIGIT_GAP;
    const float left = PANEL_CENTRE_X - width / 2.f;
    const float t = SEGMENT_THICKNESS;
    const float w = DIGIT_WIDTH;
    const float h = DIGIT_HEIGHT;
    const glm::vec3 unlit = PANEL_COLOUR + (colour - PANEL_COLOUR) * UNLIT_SEGMENT_STRENGTH;

    std::vector<SevenSegmentDisplay::Digit> digits;

    for (int i = 0; i < digitCount; ++i)
    {
        const float x = left + (float)i * (DIGIT_WIDTH + DIGIT_GAP);
        const float y = bottom;

        // Segment boxes, centre and size, in the a..g order the display uses.
        const b2Vec2 horizontal{w - t, t};
        const b2Vec2 vertical{t, h / 2.f - t};
        const std::array<std::pair<b2Vec2, b2Vec2>, 7> segments = {{
                {{x + w / 2.f, y + h - t / 2.f}, horizontal},       // a
                {{x + w - t / 2.f, y + 3.f * h / 4.f}, vertical},   // b
                {{x + w - t / 2.f, y + h / 4.f}, vertical},         // c
                {{x + w / 2.f, y + t / 2.f}, horizontal},           // d
                {{x + t / 2.f, y + h / 4.f}, vertical},             // e
                {{x + t / 2.f, y + 3.f * h / 4.f}, vertical},       // f
                {{x + w / 2.f, y + h / 2.f}, horizontal},           // g
        }};

        SevenSegmentDisplay::Digit digit{};
        for (int s = 0; s < 7; ++s)
            digit[s] = addBoxRenderer(game, segments[s].first, segments[s].second, unlit);
        digits.push_back(digit);
    }

    return SevenSegmentDisplay(std::move(digits), colour, unlit);
}

/// Four cells the controller moves around to draw a piece in a preview slot.
std::array<Entity*, 4> addPreviewCells(World& game)
{
    const float size = layout::CELL_SIZE * layout::PREVIEW_SCALE;
    std::array<Entity*, 4> cells{};

    for (Entity*& cell: cells)
    {
        cell = addBox(game, b2Vec2{0.f, 0.f}, b2Vec2{size, size}, layout::EMPTY_CELL_COLOUR);
        cell->getComponent<BoxRenderer>()->setVisible(false);
    }

    return cells;
}

} // namespace

void createTetrisScene(World& game)
{
    using namespace layout;

    game.setView(VIEW_LEFT, VIEW_RIGHT, VIEW_BOTTOM, VIEW_TOP);
    game.setClearColour(BACKGROUND_COLOUR.r, BACKGROUND_COLOUR.g, BACKGROUND_COLOUR.b);
    b2World_SetGravity(game.getPhysics().getWorld(), DEBRIS_GRAVITY);

    TetrisViews views;

    // Entities draw in the order they were added, so backgrounds go first.
    const b2Vec2 wellCentre{(float)BOARD_WIDTH / 2.f, (float)VISIBLE_HEIGHT / 2.f};
    addBox(game, wellCentre, b2Vec2{(float)BOARD_WIDTH + 0.4f, (float)VISIBLE_HEIGHT + 0.4f}, FRAME_COLOUR);
    addBox(game, wellCentre, b2Vec2{(float)BOARD_WIDTH, (float)VISIBLE_HEIGHT}, WELL_COLOUR);

    for (int y = 0; y < VISIBLE_HEIGHT; ++y)
        for (int x = 0; x < BOARD_WIDTH; ++x)
            views.board[y * BOARD_WIDTH + x] =
                    addBoxRenderer(game, cellCentre(x, y), b2Vec2{CELL_SIZE, CELL_SIZE}, EMPTY_CELL_COLOUR);

    addPanel(game, NEXT_PANEL_CENTRE, NEXT_PANEL_SIZE);
    addPanel(game, HOLD_PANEL_CENTRE, HOLD_PANEL_SIZE);
    addPanel(game, SCORE_PANEL_CENTRE, SCORE_PANEL_SIZE);

    for (auto& slot: views.next)
        slot = addPreviewCells(game);
    views.hold = addPreviewCells(game);

    views.score = addReadout(game, SCORE_DIGITS, SCORE_DIGITS_Y, SCORE_COLOUR);
    views.lines = addReadout(game, LINES_DIGITS, LINES_DIGITS_Y, LINES_COLOUR);
    views.level = addReadout(game, LEVEL_DIGITS, LEVEL_DIGITS_Y, LEVEL_COLOUR);

    // The controller lives on an entity of its own, out of sight. Every
    // entity carries a body, so give it a tiny sensor nobody will notice.
    PhysicsMaterial material;
    material.isSensor = true;
    auto controller = new Entity(game.getPhysics().getWorld(), b2Vec2{-0.5f, -0.5f},
                                 b2Vec2{-0.49f, -0.51f}, BodyType::Static, material);
    controller->setName("tetris");
    controller->addComponent(new TetrisController(*controller, std::move(views)));
    game.addEntity(controller);
}
