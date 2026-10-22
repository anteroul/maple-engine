// Headless checks of the Tetris rules. No window, no GL, no physics.
#include "../games/tetris/TetrisGame.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

using namespace tetris;

static int failures = 0;

static void check(bool condition, const char* what)
{
    printf("%s  %s\n", condition ? "[ ok ]" : "[FAIL]", what);
    if (!condition) failures++;
}

static bool inBounds(const Piece& piece)
{
    for (const Point& cell: piece.cells())
        if (cell.x < 0 || cell.x >= BOARD_WIDTH || cell.y < 0 || cell.y >= BOARD_HEIGHT)
            return false;
    return true;
}

static int lowestRow(const Piece& piece)
{
    int lowest = BOARD_HEIGHT;
    for (const Point& cell: piece.cells())
        lowest = std::min(lowest, cell.y);
    return lowest;
}

static int highestRow(const Piece& piece)
{
    int highest = -1;
    for (const Point& cell: piece.cells())
        highest = std::max(highest, cell.y);
    return highest;
}

static bool boardEmpty(const Game& game)
{
    for (int y = 0; y < BOARD_HEIGHT; ++y)
        for (int x = 0; x < BOARD_WIDTH; ++x)
            if (game.cellAt(x, y))
                return false;
    return true;
}

static void testBag()
{
    // Lock pieces by hard dropping them and record what spawns. Each run of
    // seven must contain every piece exactly once.
    Game game(1234);
    bool everyBagComplete = true;

    for (int bag = 0; bag < 2; ++bag)
    {
        std::set<PieceType> seen;
        for (int i = 0; i < PIECE_TYPE_COUNT; ++i)
        {
            seen.insert(game.activePiece().type);
            game.hardDrop();
            // Keep the stack from reaching the top: wipe it after each drop.
            for (int y = 0; y < BOARD_HEIGHT; ++y)
                for (int x = 0; x < BOARD_WIDTH; ++x)
                    game.setCell(x, y, std::nullopt);
        }
        everyBagComplete = everyBagComplete && seen.size() == PIECE_TYPE_COUNT;
    }

    check(everyBagComplete, "7-bag: every run of seven pieces holds each piece once");
}

static void testSpawn()
{
    for (int i = 0; i < PIECE_TYPE_COUNT; ++i)
    {
        Game game(1);
        game.setActivePiece((PieceType)i);
        const Piece& piece = game.activePiece();

        if (highestRow(piece) != VISIBLE_HEIGHT - 1 || lowestRow(piece) < VISIBLE_HEIGHT - 2)
        {
            check(false, "pieces spawn in the top two visible rows");
            return;
        }
    }
    check(true, "pieces spawn in the top two visible rows");
}

static void testMovementStopsAtWalls()
{
    Game game(1);
    game.setActivePiece(PieceType::T);

    int moves = 0;
    while (game.moveLeft())
        moves++;

    int leftmost = BOARD_WIDTH;
    for (const Point& cell: game.activePiece().cells())
        leftmost = std::min(leftmost, cell.x);

    check(leftmost == 0 && moves == 3, "a T slides three columns left and stops at the wall");

    while (game.moveRight()) {}
    int rightmost = -1;
    for (const Point& cell: game.activePiece().cells())
        rightmost = std::max(rightmost, cell.x);

    check(rightmost == BOARD_WIDTH - 1, "and stops at the right wall too");
}

static void testRotation()
{
    for (int i = 0; i < PIECE_TYPE_COUNT; ++i)
    {
        const auto type = (PieceType)i;
        if (pieceShape(type, 0) != pieceShape(type, 4))
        {
            check(false, "four quarter turns bring every piece back to its spawn shape");
            return;
        }
    }
    check(true, "four quarter turns bring every piece back to its spawn shape");

    Game game(1);
    game.setActivePiece(PieceType::T);
    const PieceCells before = game.activePiece().cells();
    game.rotateClockwise();
    game.rotateCounterClockwise();
    check(game.activePiece().cells() == before, "clockwise then counter-clockwise is a no-op in open space");

    // SRS: an I standing on its right edge against the left wall must kick
    // back into the well when turned flat.
    game.setActivePiece(PieceType::I);
    game.rotateCounterClockwise();
    while (game.moveLeft()) {}
    const bool rotated = game.rotateClockwise();
    check(rotated && inBounds(game.activePiece()), "a vertical I against the wall kicks when rotated");

    // A T rotating into a gap uses the SRS kicks rather than failing.
    Game spin(1);
    spin.setActivePiece(PieceType::T);
    check(spin.rotateClockwise() && spin.rotateClockwise() && spin.rotateClockwise() && spin.rotateClockwise(),
          "a T can rotate all the way round in open space");
}

static void testHardDropAndLineClear()
{
    Game game(1);
    // Row 0 full except for the four columns an I spawns over (3 to 6).
    for (int x = 0; x < BOARD_WIDTH; ++x)
        if (x < 3 || x > 6)
            game.setCell(x, 0, PieceType::J);

    game.setActivePiece(PieceType::I);
    const int rows = game.hardDrop();
    const auto cleared = game.takeClearedCells();

    check(rows == VISIBLE_HEIGHT - 1, "a flat I hard drops from its spawn row to the floor");
    check(game.lines() == 1, "filling a row clears it");
    check(game.score() == 100 + 2 * (std::uint32_t)rows, "single: 100 points, plus 2 per row hard dropped");
    check(cleared.size() == BOARD_WIDTH, "the cleared row's cells are reported for effects");
    check(boardEmpty(game), "nothing is left on the board after the clear");
    check(game.takeClearedCells().empty(), "takeClearedCells() empties the list");
}

static void testTetris()
{
    Game game(1);
    // Four rows full except column 9.
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < BOARD_WIDTH - 1; ++x)
            game.setCell(x, y, PieceType::L);

    game.setActivePiece(PieceType::I);
    game.rotateClockwise();
    while (game.moveRight()) {}
    const int rows = game.hardDrop();

    check(game.lines() == 4, "a vertical I down the side clears four rows");
    check(game.score() == 800 + 2 * (std::uint32_t)rows, "tetris: 800 points");
    check(boardEmpty(game), "the well is empty again");
}

static void testRowsAboveFallAfterClear()
{
    Game game(1);
    for (int x = 0; x < BOARD_WIDTH; ++x)
        if (x < 3 || x > 6)
            game.setCell(x, 0, PieceType::J);
    game.setCell(0, 1, PieceType::Z);
    game.setCell(0, 5, PieceType::S);

    game.setActivePiece(PieceType::I);
    game.hardDrop();

    check(game.cellAt(0, 0) == PieceType::Z && game.cellAt(0, 4) == PieceType::S
          && !game.cellAt(0, 5),
          "rows above a clear move down by one, gaps and all");
}

static void testGravityAndLockDelay()
{
    Game game(1);
    game.setActivePiece(PieceType::O);
    const int startY = game.activePiece().origin.y;

    game.update(game.gravityInterval() * 0.5f, false);
    check(game.activePiece().origin.y == startY, "half a gravity interval does not move the piece");

    game.update(game.gravityInterval() * 0.6f, false);
    check(game.activePiece().origin.y == startY - 1, "a full interval drops it one row");

    Game soft(1);
    soft.setActivePiece(PieceType::O);
    soft.update(soft.gravityInterval() / SOFT_DROP_FACTOR * 1.01f, true);
    check(soft.activePiece().origin.y == startY - 1 && soft.score() == 1,
          "soft drop falls twenty times faster and scores a point per row");

    // Let it fall to the floor and sit there.
    for (int i = 0; i < 40; ++i)
        game.update(game.gravityInterval(), false);
    check(game.activePiece().type != PieceType::O || game.activePiece().origin.y > 0,
          "a grounded piece locks once the lock delay runs out");
    check(game.cellAt(4, 0) == PieceType::O && game.cellAt(5, 1) == PieceType::O,
          "the locked O is on the floor where it landed");

    // Moving a grounded piece buys it more time.
    Game slide(1);
    slide.setActivePiece(PieceType::T);
    while (slide.activePiece().origin.y > slide.ghostPiece().origin.y)
        slide.update(slide.gravityInterval(), false);
    slide.update(LOCK_DELAY * 0.8f, false);
    check(slide.moveLeft(), "a grounded piece can still slide");
    const PieceCells landed = slide.activePiece().cells();
    slide.update(LOCK_DELAY * 0.8f, false);
    check(slide.activePiece().cells() == landed, "a move while grounded restarts the lock delay");
    slide.update(LOCK_DELAY * 0.3f, false);
    bool locked = true;
    for (const Point& cell: landed)
        locked = locked && slide.cellAt(cell.x, cell.y) == PieceType::T;
    check(locked, "and the piece locks once the delay runs out after it");
}

static void testHold()
{
    Game game(99);
    const PieceType first = game.activePiece().type;
    const PieceType upcoming = game.nextPieces()[0];

    check(game.hold(), "the first hold works");
    check(game.heldPiece() == first && game.activePiece().type == upcoming,
          "holding stores the piece and brings in the next one");
    check(!game.hold(), "a second hold before the piece locks is refused");

    game.hardDrop();
    const PieceType beforeSwap = game.activePiece().type;
    check(game.hold() && game.activePiece().type == first && game.heldPiece() == beforeSwap,
          "after a lock, hold swaps with the held piece");
}

static void testGameOver()
{
    Game game(1);
    // Fill everything but one column, so nothing clears and the spawn area
    // is blocked.
    for (int y = 0; y < VISIBLE_HEIGHT; ++y)
        for (int x = 1; x < BOARD_WIDTH; ++x)
            game.setCell(x, y, PieceType::Z);

    game.setActivePiece(PieceType::T);
    check(game.isGameOver(), "a piece that cannot spawn ends the game");
    check(!game.moveLeft() && game.hardDrop() == 0, "no moves are accepted after a game over");

    game.reset();
    check(!game.isGameOver() && boardEmpty(game) && game.score() == 0, "reset starts a fresh game");
}

static void testLevels()
{
    Game game(1);
    check(game.level() == 1 && std::abs(game.gravityInterval() - 1.f) < 1e-5f,
          "level 1 drops one row per second");

    // Ten singles reach level 2.
    for (int i = 0; i < 10; ++i)
    {
        for (int x = 0; x < BOARD_WIDTH; ++x)
            if (x < 3 || x > 6)
                game.setCell(x, 0, PieceType::J);
        game.setActivePiece(PieceType::I);
        game.hardDrop();
    }
    check(game.lines() == 10 && game.level() == 2, "ten lines reach level 2");
    check(game.gravityInterval() < 1.f, "and gravity gets faster");
}

int main()
{
    testBag();
    testSpawn();
    testMovementStopsAtWalls();
    testRotation();
    testHardDropAndLineClear();
    testTetris();
    testRowsAboveFallAfterClear();
    testGravityAndLockDelay();
    testHold();
    testGameOver();
    testLevels();

    printf("\n%s (%d failure(s))\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
