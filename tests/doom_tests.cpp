// Headless checks of the Maple Doom rules. No window, no GL.
#include "../games/doom/Game.h"
#include "../games/doom/Levels.h"
#include "../games/doom/Raycast.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace doom;

static int failures = 0;

static void check(bool condition, const char* what)
{
    printf("%s  %s\n", condition ? "[ ok ]" : "[FAIL]", what);
    if (!condition) failures++;
}

static MapDefinition level(std::vector<std::string> rows)
{
    return MapDefinition{"TEST", std::move(rows), {}, 0.5f};
}

static void run(Game& game, const Input& input, float seconds)
{
    for (int i = 0; i < (int)(seconds / TICK); ++i)
        game.tick(input);
}

/// Run until `done` says so, or `seconds` pass. True if it did.
template<typename Done>
static bool runUntil(Game& game, const Input& input, float seconds, Done done)
{
    for (int i = 0; i < (int)(seconds / TICK); ++i)
    {
        game.tick(input);
        if (done())
            return true;
    }
    return false;
}

static const Thing* firstOf(const Game& game, ThingType type)
{
    for (const Thing& thing: game.things())
        if (thing.type == type && !thing.removed)
            return &thing;
    return nullptr;
}

static int countOf(const Game& game, ThingType type)
{
    int count = 0;
    for (const Thing& thing: game.things())
        count += thing.type == type && !thing.removed;
    return count;
}

static void testCampaignParses()
{
    bool parsed = true;
    bool exits = true;
    try
    {
        for (const MapDefinition& definition: campaign())
        {
            const Map map(definition);
            bool exit = false;
            for (int y = 0; y < map.height(); ++y)
                for (int x = 0; x < map.width(); ++x)
                    exit = exit || map.at(x, y).tile == Tile::Exit;
            exits = exits && exit;
        }
    }
    catch (const std::invalid_argument& error)
    {
        printf("       %s\n", error.what());
        parsed = false;
    }
    check(parsed, "every level of the campaign parses");
    check(exits, "every level has an exit switch");
}

static void testBadMapsAreRejected()
{
    auto rejected = [](std::vector<std::string> rows) {
        try
        {
            Map map(level(std::move(rows)));
            return false;
        }
        catch (const std::invalid_argument&)
        {
            return true;
        }
    };
    check(rejected({"####", "#.#", "####"}), "ragged rows are rejected");
    check(rejected({"####", "#..#", "####"}), "a map with no player start is rejected");
    check(rejected({"####", "#>?#", "####"}), "unknown letters are rejected");
}

static void testRaycast()
{
    const Map map(level({
            "#######",
            "#>....#",
            "#######",
    }));

    const RayHit east = castRay(map, 1.5f, 1.5f, 1.f, 0.f);
    check(east.hit && std::fabs(east.t - 4.5f) < 1e-4f, "a ray east meets the wall 4.5 cells away");
    check(east.cellX == 6 && east.side == 0, "it hits the west face of the far wall");

    const RayHit north = castRay(map, 1.5f, 1.5f, 0.f, -1.f);
    check(north.hit && std::fabs(north.t - 0.5f) < 1e-4f && north.side == 1, "a ray north meets the wall half a cell away");

    const RayHit diagonal = castRay(map, 1.5f, 1.5f, 1.f, 1.f);
    check(diagonal.hit && diagonal.u >= 0.f && diagonal.u <= 1.f, "texture coordinates stay between 0 and 1");

    check(!castRay(map, 1.5f, 1.5f, 1.f, 0.f, 2.f).hit, "a ray stops at its limit");
    check(lineOfSight(map, 1.5f, 1.5f, 5.5f, 1.5f), "two ends of a corridor see each other");
}

static void testDoors()
{
    Game game({level({
            "#######",
            "#>.D..#",
            "#######",
    })});

    const RayHit shut = castRay(game.map(), 1.5f, 1.5f, 1.f, 0.f);
    check(shut.tile == Tile::Door && std::fabs(shut.t - 2.f) < 1e-4f, "a shut door stands across the middle of its cell");
    check(!lineOfSight(game.map(), 1.5f, 1.5f, 5.5f, 1.5f), "a shut door blocks sight");

    Input forward;
    forward.forward = 1.f;
    run(game, forward, 1.5f);
    check(game.player().x < 3.f - PLAYER_RADIUS + 0.01f, "a shut door stops the player");

    Input use;
    use.use = true;
    game.tick(use);
    run(game, Input{}, 1.f);
    check(game.map().at(3, 1).open >= 1.f, "using a door opens it");
    check(lineOfSight(game.map(), 1.5f, 1.5f, 5.5f, 1.5f), "an open door can be seen through");

    run(game, forward, 1.f);
    check(game.player().x > 4.f, "the player walks through the open door");

    run(game, Input{}, 6.f);
    check(game.map().at(3, 1).open <= 0.f, "the door shuts again by itself");
}

static void testWallsBlockMovement()
{
    Game game({level({
            "#####",
            "#>..#",
            "#...#",
            "#####",
    })});

    Input input;
    input.forward = 1.f;
    input.strafe = -1.f;
    input.run = true;
    run(game, input, 3.f);

    const Player& player = game.player();
    check(player.x <= 4.f - PLAYER_RADIUS + 1e-3f && player.y >= 1.f + PLAYER_RADIUS - 1e-3f,
          "running into a corner never goes through the walls");
    check(player.x > 3.5f, "the player slides along the wall into the corner");
}

static void testShootingKillsMonsters()
{
    Game game({level({
            "########",
            "#>....z#",
            "########",
    })});

    Input fire;
    fire.fire = true;
    const bool killed = runUntil(game, fire, 10.f, [&] { return game.stats().kills == 1; });
    check(killed, "pistol shots kill a trooper");
    check(game.player().ammo[(int)AmmoType::Bullets] < 50, "shooting uses up bullets");

    run(game, Input{}, 1.f);
    const Thing* trooper = firstOf(game, ThingType::Trooper);
    check(trooper && trooper->state == ThingState::Dead, "the trooper ends up a corpse");
    check(countOf(game, ThingType::Clip) == 1, "the trooper drops a clip");
}

static void testBarrelsChain()
{
    Game game({level({
            "###########",
            "#>.....oo.#",
            "#........z#",
            "###########",
    })});

    Input fire;
    fire.fire = true;
    runUntil(game, fire, 5.f, [&] { return countOf(game, ThingType::Barrel) < 2; });
    run(game, Input{}, 1.f);

    check(countOf(game, ThingType::Barrel) == 0, "one barrel going off sets off its neighbour");
    check(game.stats().kills == 1, "the blast kills the trooper beside them");
}

static void testMonstersAttack()
{
    Game game({level({
            "##########",
            "#>......i#",
            "##########",
    })});

    const bool hurt = runUntil(game, Input{}, 10.f, [&] { return game.player().health < 100; });
    check(hurt, "an imp that sees the player throws fireballs at them");
}

static void testMonstersFindTheWay()
{
    // The demon starts out of sight round a wall, and has to go the long
    // way round to reach the player.
    Game game({level({
            "#########",
            "#>......#",
            "#######.#",
            "#d......#",
            "#########",
    })});

    // A shot carries round the corner and wakes it.
    Input fire;
    fire.fire = true;
    game.tick(fire);
    check(firstOf(game, ThingType::Demon)->state != ThingState::Idle, "a gunshot wakes the monster round the corner");

    const bool bitten = runUntil(game, Input{}, 20.f, [&] { return game.player().health < 100; });
    check(bitten, "the demon finds its way round and bites");
}

static void testMonstersOpenDoors()
{
    Game game({level({
            "#########",
            "#>......#",
            "#######D#",
            "#d......#",
            "#########",
    })});

    Input fire;
    fire.fire = true;
    game.tick(fire);
    check(firstOf(game, ThingType::Demon)->state == ThingState::Idle, "a shut door keeps the sound of a shot in");

    // Hurting it is another way to wake it.
    game.damage(0, 1, PLAYER);
    const bool bitten = runUntil(game, Input{}, 20.f, [&] { return game.player().health < 100; });
    check(bitten, "the demon opens the door on its way to the player");
}

static void testPickups()
{
    Game game({level({
            "#######",
            "#>+.S.#",
            "#######",
    })});

    Input forward;
    forward.forward = 1.f;
    run(game, forward, 0.4f);
    check(countOf(game, ThingType::Medkit) == 1, "a medikit is left alone at full health");

    game.damage(PLAYER, 30, WORLD);
    const int hurt = game.player().health;
    game.placePlayer(1.5f, 1.5f, 0.f);
    run(game, forward, 0.4f);
    check(countOf(game, ThingType::Medkit) == 0 && game.player().health == hurt + 25, "a medikit heals 25");

    run(game, forward, 1.f);
    check(game.player().owned[(int)Weapon::Shotgun], "walking over the shotgun picks it up");
    run(game, Input{}, 1.f);
    check(game.player().weapon == Weapon::Shotgun, "a new weapon is switched to");
    check(game.stats().items == 2 && game.stats().totalItems == 2, "pickups count towards the items tally");
}

static void testExitAndNextLevel()
{
    Game game({level({
                       "#####",
                       "#>.X#",
                       "#####",
               }),
               level({
                       "#####",
                       "#v..#",
                       "#####",
               })});

    Input forward;
    forward.forward = 1.f;
    run(game, forward, 0.5f);

    game.damage(PLAYER, 40, WORLD);
    Input use;
    use.use = true;
    game.tick(use);
    check(game.state() == GameState::LevelComplete, "using the exit switch ends the level");

    game.tick(Input{});
    game.tick(use);
    check(game.state() == GameState::Playing && game.levelIndex() == 1, "using again goes on to the next level");
    check(game.player().health == 60, "health carries over to the next level");
}

static void testDeathRestarts()
{
    Game game({level({
            "#####",
            "#>.S#",
            "#####",
    })});

    Input forward;
    forward.forward = 1.f;
    run(game, forward, 1.f);
    check(game.player().owned[(int)Weapon::Shotgun], "the shotgun is picked up");

    game.damage(PLAYER, 500, WORLD);
    check(game.player().dead, "enough damage kills the player");

    run(game, Input{}, 1.5f);
    Input use;
    use.use = true;
    game.tick(use);
    check(!game.player().dead && game.player().health == 100, "using after death starts the level over");
    check(!game.player().owned[(int)Weapon::Shotgun], "the restart is from a pistol start");
}

static void testArmorAbsorbs()
{
    Game game({level({
            "#####",
            "#>a.#",
            "#####",
    })});

    Input forward;
    forward.forward = 1.f;
    run(game, forward, 0.5f);
    check(game.player().armor == 100, "armor is picked up");

    game.damage(PLAYER, 30, WORLD);
    check(game.player().health == 80 && game.player().armor == 90, "armor soaks up a third of the damage");
}

int main()
{
    testCampaignParses();
    testBadMapsAreRejected();
    testRaycast();
    testDoors();
    testWallsBlockMovement();
    testShootingKillsMonsters();
    testBarrelsChain();
    testMonstersAttack();
    testMonstersFindTheWay();
    testMonstersOpenDoors();
    testPickups();
    testExitAndNextLevel();
    testDeathRestarts();
    testArmorAbsorbs();

    printf("\n%s\n", failures == 0 ? "All tests passed." : "Some tests FAILED.");
    return failures == 0 ? 0 : 1;
}
