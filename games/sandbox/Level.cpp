#include "Level.h"
#include <algorithm>
#include <cmath>

namespace sandbox {

namespace {

void fillRect(Simulation& sim, int x0, int y0, int x1, int y1, Material material)
{
    for (int y = std::max(y0, 0); y <= std::min(y1, sim.height() - 1); ++y)
        for (int x = std::max(x0, 0); x <= std::min(x1, sim.width() - 1); ++x)
            sim.set(x, y, material);
}

/// Only where the cell is still empty.
void fillEmpty(Simulation& sim, int x0, int y0, int x1, int y1, Material material)
{
    for (int y = std::max(y0, 0); y <= std::min(y1, sim.height() - 1); ++y)
        for (int x = std::max(x0, 0); x <= std::min(x1, sim.width() - 1); ++x)
            if (sim.materialAt(x, y) == Material::Empty)
                sim.set(x, y, material);
}

} // namespace

void buildStarterLevel(Simulation& sim)
{
    sim.clear();

    const int w = sim.width();
    const int h = sim.height();
    // Positions below are laid out on a 320 x 180 grid.
    const auto X = [w](int x) { return x * w / 320; };
    const auto Y = [h](int y) { return y * h / 180; };

    // Rolling ground.
    std::vector<int> ground(w);
    for (int x = 0; x < w; ++x)
    {
        const float u = (float)x * 320.f / (float)w;
        ground[x] = Y(14 + (int)(5.f * std::sin(u * 0.025f) + 3.f * std::sin(u * 0.09f + 1.f)));
        fillRect(sim, x, 0, x, ground[x], Material::Stone);
    }

    // A walled pool of water, with a slick of oil on top.
    const int poolLeft = X(18), poolRight = X(88);
    const int poolFloor = *std::max_element(ground.begin() + poolLeft, ground.begin() + poolRight);
    fillRect(sim, poolLeft, 0, poolLeft + X(3), poolFloor + Y(26), Material::Stone);
    fillRect(sim, poolRight - X(3), 0, poolRight, poolFloor + Y(26), Material::Stone);
    fillEmpty(sim, poolLeft + X(4), 0, poolRight - X(4), poolFloor + Y(18), Material::Water);
    fillEmpty(sim, poolLeft + X(4), 0, poolRight - X(4), poolFloor + Y(21), Material::Oil);

    // A wooden hut with a stash of gunpowder inside.
    const int hutLeft = X(122), hutRight = X(168);
    const int hutFloor = *std::max_element(ground.begin() + hutLeft, ground.begin() + hutRight) + 1;
    const int hutTop = hutFloor + Y(30);
    fillRect(sim, hutLeft, hutFloor, hutRight, hutFloor + 1, Material::Wood);
    fillRect(sim, hutLeft, hutFloor, hutLeft + 1, hutTop, Material::Wood);
    fillRect(sim, hutRight - 1, hutFloor, hutRight, hutTop, Material::Wood);
    for (int i = 0; i <= (hutRight - hutLeft) / 2 + 3; ++i)
    {
        // A pitched roof, two cells thick.
        const int left = hutLeft - 3 + i, right = hutRight + 3 - i;
        fillRect(sim, left, hutTop + i, left + 1, hutTop + i, Material::Wood);
        fillRect(sim, right - 1, hutTop + i, right, hutTop + i, Material::Wood);
    }
    for (int y = 0; y < Y(9); ++y)
    {
        // A heap, wider at the bottom.
        const int halfWidth = Y(9) - y;
        const int centre = (hutLeft + hutRight) / 2;
        fillEmpty(sim, centre - halfWidth, hutFloor + 2 + y, centre + halfWidth, hutFloor + 2 + y,
                  Material::Gunpowder);
    }

    // A sand dune.
    const int duneCentre = X(215), duneHalfWidth = X(32);
    for (int x = duneCentre - duneHalfWidth; x <= duneCentre + duneHalfWidth; ++x)
    {
        const float t = (float)(x - duneCentre) / (float)duneHalfWidth;
        const int height = (int)((float)Y(22) * (1.f - t * t));
        fillEmpty(sim, x, ground[x] + 1, x, ground[x] + height, Material::Sand);
    }

    // A wooden ledge high up, loaded with sand, over the dune.
    const int ledgeY = Y(120);
    fillRect(sim, X(180), ledgeY, X(250), ledgeY + 2, Material::Wood);
    fillRect(sim, X(180), ledgeY, X(183), ledgeY + Y(40), Material::Stone);
    fillEmpty(sim, X(184), ledgeY + 3, X(246), ledgeY + Y(10), Material::Sand);

    // A pit of lava, walled in by stone.
    const int pitLeft = X(268), pitRight = X(312);
    const int pitFloor = *std::max_element(ground.begin() + pitLeft, ground.begin() + pitRight);
    fillRect(sim, pitLeft, 0, pitLeft + X(2), pitFloor + Y(12), Material::Stone);
    fillRect(sim, pitRight - X(2), 0, pitRight, pitFloor + Y(12), Material::Stone);
    fillEmpty(sim, pitLeft + X(3), 0, pitRight - X(3), pitFloor + Y(7), Material::Lava);
}

} // namespace sandbox
