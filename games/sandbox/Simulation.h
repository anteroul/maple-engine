#ifndef MAPLESANDBOX_SIMULATION_H
#define MAPLESANDBOX_SIMULATION_H

#include <cstdint>
#include <vector>

/// A falling sand simulation in the style of Noita: every pixel is a cell of
/// some material, and every material follows a few simple local rules.
/// Powders pile up, liquids find their level, gases rise, and fire spreads
/// through whatever burns, leaving smoke and ash behind.
///
/// No engine or GL dependencies, so the tests can run it headless.
namespace sandbox {

enum class Material : std::uint8_t {
    Empty,
    Stone,
    Sand,
    Water,
    Wood,
    Oil,
    Gunpowder,
    Lava,
    Fire,
    Smoke,
    Steam,
    Ash,
    Count
};

/// How a material moves.
enum class Phase : std::uint8_t {
    None,    // Empty
    Solid,   // never moves: stone, wood
    Powder,  // falls and piles up: sand, gunpowder, ash
    Liquid,  // falls and spreads sideways: water, oil, lava
    Gas,     // rises and spreads sideways: smoke, steam
    Flame,   // free fire: flickers upwards and burns out quickly
};

struct MaterialInfo {
    const char* name;
    Phase phase;
    /// Movable materials sink through lighter liquids. Gases and flames
    /// count as 0, and anything can fall into them.
    int density;
    /// Chance per tick that a burning neighbour sets this alight. 0 never burns.
    float flammability;
    /// How many ticks it burns, once alight.
    int burnTimeMin;
    int burnTimeMax;
    /// How far a liquid can flow sideways in one tick.
    int dispersion;
};

const MaterialInfo& info(Material material);

struct Cell {
    Material material = Material::Empty;
    /// Fixed per-cell colour variation, so a pile of sand looks grainy.
    std::uint8_t shade = 0;
    /// The tick that last updated this cell, so a cell that moves along the
    /// scan direction is not moved again in the same tick.
    std::uint8_t clock = 0;
    bool burning = false;
    /// Ticks left: how long a burning cell burns, or a flame or gas lasts.
    std::int16_t life = 0;
};

struct Explosion {
    int x, y, radius;
};

struct Point {
    int x, y;
};

class Simulation {
public:
    Simulation(int width, int height, std::uint32_t seed = 1);

    int width() const { return m_Width; }
    int height() const { return m_Height; }
    bool inBounds(int x, int y) const { return x >= 0 && x < m_Width && y >= 0 && y < m_Height; }

    /// (0, 0) is the bottom left. Out of bounds reads as stone, a solid wall.
    const Cell& at(int x, int y) const;
    Material materialAt(int x, int y) const { return at(x, y).material; }

    /// Put a fresh cell of `material` at (x, y), replacing what is there.
    void set(int x, int y, Material material);
    /// Brush: fill the empty cells of a disc with `material`, each with
    /// probability `density`, so that powders and liquids spray in grainy.
    /// Fire also sets alight whatever burns in the disc. Empty erases.
    void paint(int cx, int cy, int radius, Material material, float density = 1.f);
    /// Set the cell alight if it burns. Returns whether it did.
    bool ignite(int x, int y);
    /// Blow a hole: clears the disc, fills it with fire and sets off whatever
    /// burns around it. Stone survives.
    void explode(int cx, int cy, int radius);
    void clear();

    /// Advance one tick.
    void step();

    /// Explosions since the last call, for the game to show.
    std::vector<Explosion> takeExplosions();
    /// Some of the cells that caught fire since the last call, for sparks.
    std::vector<Point> takeIgnitions();

    int count(Material material) const;
    int burningCount() const;
    std::uint32_t tick() const { return m_Tick; }

private:
    Cell& cell(int x, int y) { return m_Cells[(std::size_t)y * m_Width + x]; }

    void updateCell(int x, int y);
    /// Returns true if the cell burnt away.
    bool updateBurning(int x, int y);
    void updateFlame(int x, int y);
    void updateGas(int x, int y);
    void updateLava(int x, int y);
    void movePowder(int x, int y);
    void moveLiquid(int x, int y);

    /// Whether a cell of `mover` may move into (x, y), swapping with what is
    /// there.
    bool canDisplace(Material mover, int x, int y) const;
    /// Move (x, y) to (tx, ty), swapping the two cells.
    void swap(int x, int y, int tx, int ty);
    /// Spawn a short-lived flame in (x, y) if it is empty.
    void spawnFlame(int x, int y, int lifeMin, int lifeMax);
    /// Replace (x, y) with a flame, whatever is there.
    void spawnFlameAt(int x, int y, int lifeMin, int lifeMax);
    /// Water next to (x, y) puts it out, unless it is burning oil. Returns
    /// whether it did.
    bool quench(int x, int y);

    int randomInt(int min, int max);
    float randomFloat();
    bool chance(float probability) { return randomFloat() < probability; }

    int m_Width;
    int m_Height;
    std::vector<Cell> m_Cells;
    std::uint32_t m_Tick = 0;
    std::uint32_t m_Random;
    std::vector<Explosion> m_Explosions;
    std::vector<Point> m_Ignitions;
};

} // namespace sandbox

#endif //MAPLESANDBOX_SIMULATION_H
