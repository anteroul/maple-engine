// Headless checks of the falling sand rules. No window, no GL.
#include "../games/sandbox/Simulation.h"
#include "../games/sandbox/Level.h"
#include <cstdio>

using namespace sandbox;

static int failures = 0;

static void check(bool condition, const char* what)
{
    printf("%s  %s\n", condition ? "[ ok ]" : "[FAIL]", what);
    if (!condition) failures++;
}

static void run(Simulation& sim, int ticks)
{
    for (int i = 0; i < ticks; ++i)
        sim.step();
}

/// Lowest and highest row holding `material`, or -1 if there is none.
static int lowestRow(const Simulation& sim, Material material)
{
    for (int y = 0; y < sim.height(); ++y)
        for (int x = 0; x < sim.width(); ++x)
            if (sim.materialAt(x, y) == material)
                return y;
    return -1;
}

static int highestRow(const Simulation& sim, Material material)
{
    for (int y = sim.height() - 1; y >= 0; --y)
        for (int x = 0; x < sim.width(); ++x)
            if (sim.materialAt(x, y) == material)
                return y;
    return -1;
}

static void testSandFalls()
{
    Simulation sim(40, 40);
    sim.paint(20, 30, 4, Material::Sand);
    const int grains = sim.count(Material::Sand);
    run(sim, 200);

    check(sim.count(Material::Sand) == grains, "sand is neither created nor destroyed");
    check(lowestRow(sim, Material::Sand) == 0, "sand falls to the floor");
    check(highestRow(sim, Material::Sand) < 10, "sand piles up rather than standing in a column");
}

static void testWaterLevels()
{
    Simulation sim(40, 40);
    sim.paint(20, 30, 4, Material::Water);
    const int drops = sim.count(Material::Water);
    run(sim, 400);

    check(sim.count(Material::Water) == drops, "water is neither created nor destroyed");
    check(highestRow(sim, Material::Water) <= 2, "water spreads out flat");
}

static void testLayering()
{
    Simulation sim(30, 40);
    // Oil under water, sand on top: they should end up the other way round.
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 30; ++x)
        {
            sim.set(x, y, Material::Oil);
            sim.set(x, y + 5, Material::Water);
            sim.set(x, y + 10, Material::Sand);
        }
    run(sim, 1500);

    check(lowestRow(sim, Material::Sand) == 0, "sand sinks to the bottom through liquids");
    check(lowestRow(sim, Material::Oil) > highestRow(sim, Material::Sand),
          "oil ends up above the sand");
    // The boundary keeps moving a little, so allow a cell or two mid-swap.
    int oilUnderWater = 0;
    for (int y = 0; y + 1 < sim.height(); ++y)
        for (int x = 0; x < sim.width(); ++x)
            if (sim.materialAt(x, y) == Material::Oil && sim.materialAt(x, y + 1) == Material::Water)
                oilUnderWater++;
    check(oilUnderWater <= 2, "oil floats on water");
}

static void testFireBurnsWood()
{
    Simulation sim(40, 40);
    for (int y = 0; y < 6; ++y)
        for (int x = 5; x < 35; ++x)
            sim.set(x, y, Material::Wood);
    for (int x = 0; x < 40; ++x)
        sim.set(x, 20, Material::Stone);
    const int wood = sim.count(Material::Wood);
    const int stone = sim.count(Material::Stone);

    check(sim.ignite(5, 0), "wood can be set alight");
    check(!sim.ignite(0, 20), "stone cannot");

    run(sim, 3000);
    check(sim.count(Material::Wood) == 0, "fire spreads through all the wood and burns it away");
    check(sim.count(Material::Stone) == stone, "stone does not burn");
    check(sim.count(Material::Ash) > 0 && sim.count(Material::Ash) < wood, "burnt wood leaves some ash");
    check(sim.burningCount() == 0 && sim.count(Material::Fire) == 0, "and then the fire goes out");
}

static void testWaterPutsOutFire()
{
    Simulation sim(40, 40);
    for (int x = 0; x < 40; ++x)
    {
        sim.set(x, 0, Material::Wood);
        sim.set(x, 1, Material::Water);
    }
    for (int x = 0; x < 40; ++x)
        sim.ignite(x, 0);
    run(sim, 5);

    check(sim.burningCount() == 0, "water puts out burning wood next to it");
    check(sim.count(Material::Steam) > 0, "and boils into steam");
}

static void testOilBurnsOnWater()
{
    Simulation sim(40, 40);
    for (int x = 0; x < 40; ++x)
    {
        for (int y = 0; y < 8; ++y)
            sim.set(x, y, Material::Water);
        for (int y = 8; y < 10; ++y)
            sim.set(x, y, Material::Oil);
    }
    const int water = sim.count(Material::Water);
    sim.ignite(20, 9);
    run(sim, 600);

    check(sim.count(Material::Oil) == 0, "oil burns on water");
    check(sim.count(Material::Water) > water * 9 / 10, "without boiling the water under it away");
}

static void testSmokeRisesAndClears()
{
    Simulation sim(40, 60);
    sim.paint(20, 5, 3, Material::Smoke);
    const int start = lowestRow(sim, Material::Smoke);
    run(sim, 40);
    check(lowestRow(sim, Material::Smoke) > start, "smoke rises");
    run(sim, 400);
    check(sim.count(Material::Smoke) == 0, "and dissipates");
}

static void testGunpowderChains()
{
    Simulation sim(80, 20);
    for (int x = 2; x < 78; ++x)
        sim.set(x, 0, Material::Gunpowder);
    sim.ignite(2, 0);

    int explosions = 0;
    for (int i = 0; i < 400; ++i)
    {
        sim.step();
        explosions += (int)sim.takeExplosions().size();
    }

    check(explosions > 5, "gunpowder explodes");
    check(sim.count(Material::Gunpowder) == 0, "and one spark sets off the whole trail");
}

static void testLavaAndWater()
{
    Simulation sim(20, 20);
    for (int x = 0; x < 20; ++x)
    {
        sim.set(x, 0, Material::Lava);
        sim.set(x, 1, Material::Lava);
    }
    sim.paint(10, 10, 3, Material::Water);
    run(sim, 300);

    check(sim.count(Material::Stone) > 0, "water cools lava into stone");
    check(sim.count(Material::Steam) > 0 || sim.count(Material::Water) > 0, "and turns to steam");
}

static void testDeterministic()
{
    Simulation a(120, 70, 7), b(120, 70, 7);
    buildStarterLevel(a);
    buildStarterLevel(b);
    a.ignite(60, 20);
    b.ignite(60, 20);
    run(a, 200);
    run(b, 200);

    bool same = true;
    for (int y = 0; y < a.height() && same; ++y)
        for (int x = 0; x < a.width() && same; ++x)
            same = a.materialAt(x, y) == b.materialAt(x, y);
    check(same, "the same seed plays out the same way");
}

static void testStarterLevel()
{
    Simulation sim(320, 180);
    buildStarterLevel(sim);
    check(sim.count(Material::Water) > 0 && sim.count(Material::Oil) > 0
          && sim.count(Material::Gunpowder) > 0 && sim.count(Material::Lava) > 0
          && sim.count(Material::Wood) > 0 && sim.count(Material::Sand) > 0,
          "the starter level has every material to play with");

    const int water = sim.count(Material::Water);
    run(sim, 300);
    check(sim.count(Material::Water) > water * 9 / 10, "and its pool holds its water");
}

int main()
{
    testSandFalls();
    testWaterLevels();
    testLayering();
    testFireBurnsWood();
    testWaterPutsOutFire();
    testOilBurnsOnWater();
    testSmokeRisesAndClears();
    testGunpowderChains();
    testLavaAndWater();
    testDeterministic();
    testStarterLevel();

    printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
