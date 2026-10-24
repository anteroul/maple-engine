#include "Simulation.h"
#include <algorithm>
#include <array>
#include <utility>

namespace sandbox {

namespace {

const std::array<MaterialInfo, (std::size_t)Material::Count> MATERIALS = {{
        // name        phase          density flammability burn time  dispersion
        {"Empty",      Phase::None,   0,      0.f,         0,   0,    0},
        {"Stone",      Phase::Solid,  100,    0.f,         0,   0,    0},
        {"Sand",       Phase::Powder, 5,      0.f,         0,   0,    0},
        {"Water",      Phase::Liquid, 3,      0.f,         0,   0,    5},
        {"Wood",       Phase::Solid,  100,    0.03f,       120, 260,  0},
        {"Oil",        Phase::Liquid, 2,      0.35f,       25,  60,   4},
        {"Gunpowder",  Phase::Powder, 4,      0.8f,        2,   6,    0},
        {"Lava",       Phase::Liquid, 6,      0.f,         0,   0,    1},
        {"Fire",       Phase::Flame,  0,      0.f,         0,   0,    0},
        {"Smoke",      Phase::Gas,    0,      0.f,         0,   0,    0},
        {"Steam",      Phase::Gas,    0,      0.f,         0,   0,    0},
        {"Ash",        Phase::Powder, 2,      0.f,         0,   0,    0},
}};

constexpr std::array<Point, 4> SIDES = {{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};
constexpr std::array<Point, 8> AROUND = {{{-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                                          {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};

/// Cap on the events kept between takes, so a burning field nobody reads
/// from cannot grow them without bound.
constexpr std::size_t MAX_EVENTS = 256;

Phase phaseOf(Material material)
{
    return info(material).phase;
}

/// Anything a flame or gas can move into, and a falling cell can fall into.
bool isGaseous(Material material)
{
    const Phase phase = phaseOf(material);
    return phase == Phase::Gas || phase == Phase::Flame;
}

} // namespace

const MaterialInfo& info(Material material)
{
    return MATERIALS[(std::size_t)material];
}

Simulation::Simulation(int width, int height, std::uint32_t seed)
    : m_Width(width), m_Height(height), m_Cells((std::size_t)width * height),
      m_Random(seed ? seed : 1)
{}

const Cell& Simulation::at(int x, int y) const
{
    static const Cell WALL{Material::Stone};
    if (!inBounds(x, y))
        return WALL;
    return m_Cells[(std::size_t)y * m_Width + x];
}

int Simulation::randomInt(int min, int max)
{
    // xorshift32: plenty for this, and much cheaper than <random>, which
    // matters at several random numbers per cell per tick.
    m_Random ^= m_Random << 13;
    m_Random ^= m_Random >> 17;
    m_Random ^= m_Random << 5;
    return min + (int)(m_Random % (std::uint32_t)(max - min + 1));
}

float Simulation::randomFloat()
{
    return (float)randomInt(0, 0xFFFF) / 65536.f;
}

void Simulation::set(int x, int y, Material material)
{
    if (!inBounds(x, y))
        return;

    Cell& c = cell(x, y);
    c.material = material;
    c.shade = (std::uint8_t)randomInt(0, 255);
    // Stamped with this tick, so that a cell created mid-tick, ahead of the
    // scan, waits for the next one.
    c.clock = (std::uint8_t)m_Tick;
    c.burning = false;

    switch (material)
    {
        case Material::Fire:  c.life = (std::int16_t)randomInt(20, 50); break;
        case Material::Smoke: c.life = (std::int16_t)randomInt(120, 300); break;
        case Material::Steam: c.life = (std::int16_t)randomInt(150, 300); break;
        default:              c.life = 0; break;
    }
}

void Simulation::paint(int cx, int cy, int radius, Material material, float density)
{
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int x = cx + dx;
            const int y = cy + dy;
            if (dx * dx + dy * dy > radius * radius || !inBounds(x, y))
                continue;

            const Material current = cell(x, y).material;
            if (material == Material::Empty)
                set(x, y, Material::Empty);
            else if (material == Material::Fire && info(current).flammability > 0.f)
                ignite(x, y);
            else if (current == Material::Empty && chance(density))
                set(x, y, material);
        }
}

bool Simulation::ignite(int x, int y)
{
    if (!inBounds(x, y))
        return false;

    Cell& c = cell(x, y);
    const MaterialInfo& material = info(c.material);
    if (material.flammability <= 0.f || c.burning)
        return false;

    c.burning = true;
    c.life = (std::int16_t)randomInt(material.burnTimeMin, material.burnTimeMax);
    if (m_Ignitions.size() < MAX_EVENTS)
        m_Ignitions.push_back({x, y});
    return true;
}

void Simulation::explode(int cx, int cy, int radius)
{
    if (m_Explosions.size() < MAX_EVENTS)
        m_Explosions.push_back({cx, cy, radius});

    const int core = radius * 6 / 10;

    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int x = cx + dx;
            const int y = cy + dy;
            const int distance = dx * dx + dy * dy;
            if (distance > radius * radius || !inBounds(x, y))
                continue;

            Cell& c = cell(x, y);
            if (c.material == Material::Stone)
                continue;

            // Gunpowder in the blast goes off a moment later rather than
            // vanishing, which is what lets a trail of it chain.
            if (c.material == Material::Gunpowder)
            {
                if (!c.burning)
                {
                    ignite(x, y);
                    c.life = (std::int16_t)randomInt(1, 4);
                }
                continue;
            }

            if (distance <= core * core)
            {
                if (chance(0.5f))
                    spawnFlameAt(x, y, 8, 22);
                else
                    set(x, y, Material::Empty);
            }
            else if (!ignite(x, y) && c.material == Material::Empty && chance(0.3f))
            {
                spawnFlameAt(x, y, 5, 15);
            }
        }
}

void Simulation::clear()
{
    std::fill(m_Cells.begin(), m_Cells.end(), Cell{});
    m_Explosions.clear();
    m_Ignitions.clear();
}

std::vector<Explosion> Simulation::takeExplosions()
{
    return std::exchange(m_Explosions, {});
}

std::vector<Point> Simulation::takeIgnitions()
{
    return std::exchange(m_Ignitions, {});
}

int Simulation::count(Material material) const
{
    return (int)std::count_if(m_Cells.begin(), m_Cells.end(),
                              [material](const Cell& c) { return c.material == material; });
}

int Simulation::burningCount() const
{
    return (int)std::count_if(m_Cells.begin(), m_Cells.end(), [](const Cell& c) { return c.burning; });
}

/// Scan bottom to top, so that a falling cell lands in a row that is already
/// done, and alternate the horizontal direction every tick, so that nothing
/// drifts to one side.
void Simulation::step()
{
    ++m_Tick;
    const bool leftToRight = (m_Tick & 1) != 0;

    for (int y = 0; y < m_Height; ++y)
        for (int i = 0; i < m_Width; ++i)
            updateCell(leftToRight ? i : m_Width - 1 - i, y);
}

void Simulation::updateCell(int x, int y)
{
    Cell& c = cell(x, y);
    const auto clock = (std::uint8_t)m_Tick;
    if (c.material == Material::Empty || c.clock == clock)
        return;
    c.clock = clock;

    if (c.burning && updateBurning(x, y))
        return;

    if (c.material == Material::Lava)
    {
        updateLava(x, y);
        return;
    }

    switch (phaseOf(c.material))
    {
        case Phase::Powder: movePowder(x, y); break;
        case Phase::Liquid: moveLiquid(x, y); break;
        case Phase::Gas:    updateGas(x, y); break;
        case Phase::Flame:  updateFlame(x, y); break;
        default: break;
    }
}

bool Simulation::canDisplace(Material mover, int x, int y) const
{
    if (!inBounds(x, y))
        return false;

    const Material target = at(x, y).material;
    if (target == Material::Empty)
        return true;

    const Phase moverPhase = phaseOf(mover);
    const bool falls = moverPhase == Phase::Powder || moverPhase == Phase::Liquid;
    if (!falls)
        return false;

    if (isGaseous(target))
        return true;
    return phaseOf(target) == Phase::Liquid && info(mover).density > info(target).density;
}

void Simulation::swap(int x, int y, int tx, int ty)
{
    std::swap(cell(x, y), cell(tx, ty));
    cell(tx, ty).clock = (std::uint8_t)m_Tick;
}

void Simulation::spawnFlame(int x, int y, int lifeMin, int lifeMax)
{
    if (inBounds(x, y) && cell(x, y).material == Material::Empty)
        spawnFlameAt(x, y, lifeMin, lifeMax);
}

void Simulation::spawnFlameAt(int x, int y, int lifeMin, int lifeMax)
{
    set(x, y, Material::Fire);
    cell(x, y).life = (std::int16_t)randomInt(lifeMin, lifeMax);
}

bool Simulation::quench(int x, int y)
{
    // Oil floats on water and burns on it: water does not put out an oil
    // fire. Letting it would also be ruinous here, since the oil would catch
    // again from its neighbours at once and boil the pool under it away.
    if (cell(x, y).material == Material::Oil)
        return false;

    for (const Point side: SIDES)
    {
        const int nx = x + side.x;
        const int ny = y + side.y;
        if (materialAt(nx, ny) != Material::Water)
            continue;

        if (chance(0.2f))
            set(nx, ny, Material::Steam);

        Cell& c = cell(x, y);
        if (c.material == Material::Fire)
            set(x, y, Material::Empty);
        else
            c.burning = false;
        return true;
    }
    return false;
}

bool Simulation::updateBurning(int x, int y)
{
    if (quench(x, y))
        return false;

    Cell& c = cell(x, y);
    c.life--;

    if (c.material == Material::Gunpowder && c.life <= 0)
    {
        // Gone before the blast, or the blast would find it still there,
        // burning, and it would go off again every tick.
        set(x, y, Material::Empty);
        explode(x, y, randomInt(3, 5));
        return true;
    }

    for (const Point offset: AROUND)
    {
        const int nx = x + offset.x;
        const int ny = y + offset.y;
        const float flammability = info(materialAt(nx, ny)).flammability;
        if (flammability > 0.f && chance(flammability))
            ignite(nx, ny);
    }

    // Flames lick up out of whatever is burning, with now and then a wisp of
    // smoke instead.
    if (chance(0.35f))
        spawnFlame(x + randomInt(-1, 1), y + 1, 6, 18);
    else if (chance(0.03f) && inBounds(x, y + 1) && materialAt(x, y + 1) == Material::Empty)
        set(x, y + 1, Material::Smoke);

    if (c.life > 0)
        return false;

    if (c.material == Material::Wood)
        set(x, y, chance(0.45f) ? Material::Ash : chance(0.5f) ? Material::Smoke : Material::Empty);
    else
        set(x, y, chance(0.4f) ? Material::Smoke : Material::Empty);
    return true;
}

void Simulation::updateFlame(int x, int y)
{
    if (quench(x, y))
        return;

    Cell& c = cell(x, y);
    if (--c.life <= 0)
    {
        set(x, y, chance(0.3f) ? Material::Smoke : Material::Empty);
        return;
    }

    for (const Point offset: AROUND)
    {
        const int nx = x + offset.x;
        const int ny = y + offset.y;
        const float flammability = info(materialAt(nx, ny)).flammability;
        if (flammability > 0.f && chance(flammability * 0.5f))
            ignite(nx, ny);
    }

    // Flicker upwards.
    if (chance(0.6f))
    {
        const int tx = x + randomInt(-1, 1);
        if (inBounds(tx, y + 1) && materialAt(tx, y + 1) == Material::Empty)
            swap(x, y, tx, y + 1);
    }
}

void Simulation::updateGas(int x, int y)
{
    Cell& c = cell(x, y);
    if (--c.life <= 0)
    {
        // Steam that cools condenses, so steam that reaches a ceiling rains.
        const bool condenses = c.material == Material::Steam && chance(0.35f);
        set(x, y, condenses ? Material::Water : Material::Empty);
        return;
    }

    // Drift lazily rather than shooting straight up.
    if (chance(0.2f))
        return;

    const int dx = randomInt(-1, 1);
    const auto rise = [&](int tx, int ty) {
        if (!inBounds(tx, ty))
            return false;
        const Material target = materialAt(tx, ty);
        // Bubble up through liquids, too.
        if (target == Material::Empty || (phaseOf(target) == Phase::Liquid && chance(0.5f)))
        {
            swap(x, y, tx, ty);
            return true;
        }
        return false;
    };

    if (rise(x + dx, y + 1) || rise(x, y + 1))
        return;

    const int side = chance(0.5f) ? 1 : -1;
    if (inBounds(x + side, y) && materialAt(x + side, y) == Material::Empty)
        swap(x, y, x + side, y);
}

void Simulation::updateLava(int x, int y)
{
    // Water cools lava to stone, boiling away as it does.
    for (const Point side: SIDES)
    {
        const int nx = x + side.x;
        const int ny = y + side.y;
        if (materialAt(nx, ny) == Material::Water)
        {
            set(nx, ny, Material::Steam);
            set(x, y, Material::Stone);
            return;
        }
    }

    for (const Point offset: AROUND)
    {
        const int nx = x + offset.x;
        const int ny = y + offset.y;
        const float flammability = info(materialAt(nx, ny)).flammability;
        if (flammability > 0.f && chance(flammability * 0.5f))
            ignite(nx, ny);
    }

    if (chance(0.004f))
        spawnFlame(x, y + 1, 5, 12);

    // Thick: it only flows some of the time.
    if (chance(0.35f))
        moveLiquid(x, y);
}

void Simulation::movePowder(int x, int y)
{
    const Material material = cell(x, y).material;

    // Sinking through a liquid is slower than falling through air.
    if (canDisplace(material, x, y - 1)
        && (phaseOf(materialAt(x, y - 1)) != Phase::Liquid || chance(0.5f)))
    {
        swap(x, y, x, y - 1);
        return;
    }

    const int side = chance(0.5f) ? 1 : -1;
    if (canDisplace(material, x + side, y - 1))
        swap(x, y, x + side, y - 1);
    else if (canDisplace(material, x - side, y - 1))
        swap(x, y, x - side, y - 1);
}

void Simulation::moveLiquid(int x, int y)
{
    const Material material = cell(x, y).material;

    if (canDisplace(material, x, y - 1))
    {
        swap(x, y, x, y - 1);
        return;
    }

    const int side = chance(0.5f) ? 1 : -1;
    if (canDisplace(material, x + side, y - 1))
    {
        swap(x, y, x + side, y - 1);
        return;
    }
    if (canDisplace(material, x - side, y - 1))
    {
        swap(x, y, x - side, y - 1);
        return;
    }

    // Flow sideways, as far as the liquid's dispersion through open space.
    const int dispersion = info(material).dispersion;
    for (const int direction: {side, -side})
    {
        int distance = 0;
        for (int i = 1; i <= dispersion; ++i)
        {
            const int tx = x + direction * i;
            if (!inBounds(tx, y))
                break;
            const Material target = materialAt(tx, y);
            if (target != Material::Empty && !isGaseous(target))
                break;
            distance = i;
        }

        if (distance > 0)
        {
            swap(x, y, x + direction * distance, y);
            return;
        }
    }

    // Now and then edge into a lighter liquid alongside, so that where two
    // liquids meet the boundary settles flat instead of staying lumpy.
    const Material beside = materialAt(x + side, y);
    if (phaseOf(beside) == Phase::Liquid && info(beside).density < info(material).density
        && inBounds(x + side, y) && chance(0.3f))
        swap(x, y, x + side, y);
}

} // namespace sandbox
