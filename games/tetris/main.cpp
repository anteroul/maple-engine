/**
 * Maple Tetris: a Tetris clone built on Maple Engine.
 *
 * Controls:
 *   Left / Right, A / D     move
 *   Down, S                 soft drop
 *   Space                   hard drop
 *   Up, X, W                rotate clockwise
 *   Z, Ctrl                 rotate counter-clockwise
 *   C, Shift                hold
 *   P                       pause
 *   R                       restart
 *   M                       music on / off
 *   Esc                     quit
 */
#include "App.h"
#include "TetrisScene.h"
#include "TetrisLayout.h"

int main()
{
    using namespace tetris::layout;

    // Match the window to the view's aspect ratio, so it opens without
    // letterboxing: 18 x 22 cells at 30 pixels a cell.
    constexpr float PIXELS_PER_CELL = 30.f;
    App app((unsigned)((VIEW_RIGHT - VIEW_LEFT) * PIXELS_PER_CELL),
            (unsigned)((VIEW_TOP - VIEW_BOTTOM) * PIXELS_PER_CELL),
            "Maple Tetris", false);

    World::getInstance().setScene(createTetrisScene);
    app.Launch();
    return 0;
}
