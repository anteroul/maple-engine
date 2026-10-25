#include "Map.h"
#include "Raycast.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace doom {

namespace {

/// How far a lamp lights, in cells.
constexpr float LAMP_RADIUS = 5.5f;
constexpr float LAMP_BRIGHTNESS = 0.85f;
/// Daylight under open sky, and how far it spills in under a roof.
constexpr float SKY_LIGHT = 1.f;
constexpr float SKY_SPILL_RADIUS = 2.5f;
constexpr float SKY_SPILL = 0.35f;
/// The faint green glow nukage gives off.
constexpr float NUKAGE_RADIUS = 2.f;
constexpr float NUKAGE_GLOW = 0.3f;

bool wallLetter(char c, WallTexture& texture)
{
    switch (c)
    {
        case '#': texture = WallTexture::Stone; return true;
        case 'B': texture = WallTexture::Brick; return true;
        case 'T': texture = WallTexture::Tech; return true;
        case 'W': texture = WallTexture::Wood; return true;
        case 'H': texture = WallTexture::Hell; return true;
        default: return false;
    }
}

bool thingLetter(char c, ThingType& type)
{
    switch (c)
    {
        case 'z': type = ThingType::Trooper; return true;
        case 'i': type = ThingType::Imp; return true;
        case 'd': type = ThingType::Demon; return true;
        case 'K': type = ThingType::Knight; return true;
        case 'o': type = ThingType::Barrel; return true;
        case 'l': type = ThingType::Lamp; return true;
        case '|': type = ThingType::Column; return true;
        case '+': type = ThingType::Medkit; return true;
        case 'h': type = ThingType::Stimpack; return true;
        case 'a': type = ThingType::Armor; return true;
        case 'c': type = ThingType::Clip; return true;
        case 's': type = ThingType::Shells; return true;
        case 'r': type = ThingType::RocketBox; return true;
        case 'S': type = ThingType::Shotgun; return true;
        case 'C': type = ThingType::Chaingun; return true;
        case 'R': type = ThingType::RocketLauncher; return true;
        default: return false;
    }
}

bool startLetter(char c, float& angle)
{
    constexpr float quarter = std::numbers::pi_v<float> / 2.f;
    switch (c)
    {
        case '>': angle = 0.f; return true;
        case 'v': angle = quarter; return true;
        case '<': angle = 2.f * quarter; return true;
        case '^': angle = -quarter; return true;
        default: return false;
    }
}

} // namespace

Map::Map(const MapDefinition& definition)
    : m_Name(definition.name)
{
    if (definition.rows.empty() || definition.rows[0].empty())
        throw std::invalid_argument("map '" + m_Name + "' is empty");

    m_Height = (int)definition.rows.size();
    m_Width = (int)definition.rows[0].size();
    m_Cells.resize((std::size_t)m_Width * m_Height);

    bool started = false;

    for (int y = 0; y < m_Height; ++y)
    {
        const std::string& row = definition.rows[y];
        if ((int)row.size() != m_Width)
            throw std::invalid_argument("map '" + m_Name + "' row " + std::to_string(y) + " is "
                                        + std::to_string(row.size()) + " long, not "
                                        + std::to_string(m_Width));

        for (int x = 0; x < m_Width; ++x)
        {
            const char c = row[x];
            Cell& cell = at(x, y);
            WallTexture texture;
            ThingType type;
            float angle;

            if (wallLetter(c, texture))
            {
                cell.tile = Tile::Wall;
                cell.texture = texture;
            }
            else if (c == 'X')
            {
                cell.tile = Tile::Exit;
                cell.texture = WallTexture::Exit;
            }
            else if (c == 'D')
            {
                cell.tile = Tile::Door;
                cell.texture = WallTexture::Door;
            }
            else if (thingLetter(c, type))
            {
                m_Spawns.push_back({type, (float)x + 0.5f, (float)y + 0.5f});
            }
            else if (startLetter(c, angle))
            {
                if (started)
                    throw std::invalid_argument("map '" + m_Name + "' has more than one player start");
                started = true;
                m_StartX = (float)x + 0.5f;
                m_StartY = (float)y + 0.5f;
                m_StartAngle = angle;
            }
            else if (c != '.' && c != ' ')
            {
                throw std::invalid_argument("map '" + m_Name + "' has an unknown letter '"
                                            + std::string(1, c) + "' at " + std::to_string(x)
                                            + ", " + std::to_string(y));
            }
        }
    }

    if (!started)
        throw std::invalid_argument("map '" + m_Name + "' has no player start");

    for (const FloorZone& zone: definition.zones)
        for (int y = std::max(zone.y0, 0); y <= std::min(zone.y1, m_Height - 1); ++y)
            for (int x = std::max(zone.x0, 0); x <= std::min(zone.x1, m_Width - 1); ++x)
            {
                at(x, y).floor = zone.floor;
                if (zone.floor == Floor::Outdoor)
                    at(x, y).outdoors = true;
                else if (zone.floor != Floor::Nukage)
                    at(x, y).outdoors = false;
            }

    // A door runs between the walls either side of it.
    for (int y = 0; y < m_Height; ++y)
        for (int x = 0; x < m_Width; ++x)
            if (at(x, y).tile == Tile::Door)
                at(x, y).doorAlongX = isWall(x - 1, y) && isWall(x + 1, y);

    computeLight(definition.ambient);
}

bool Map::blocksMovement(int x, int y) const
{
    if (!inside(x, y))
        return true;

    const Cell& cell = at(x, y);
    switch (cell.tile)
    {
        case Tile::Empty: return false;
        case Tile::Door: return cell.open < 0.9f;
        default: return true;
    }
}

bool Map::isWall(int x, int y) const
{
    return !inside(x, y) || at(x, y).tile == Tile::Wall || at(x, y).tile == Tile::Exit;
}

float Map::light(int x, int y) const
{
    if (!inside(x, y))
        return 0.f;
    return m_Light[(std::size_t)y * m_Width + x];
}

/// Bake the light once, as the level loads: ambient light, plus lamps, sky
/// and nukage lighting whatever they can see. Walls cast shadows, doors too,
/// since they are shut while this runs.
void Map::computeLight(float ambient)
{
    // Lamps add up. Sky and nukage come from whole areas of cells, and only
    // the brightest of those counts, or a cell beside a big pool of nukage
    // would outshine one beside a small one many times over.
    std::vector<float> lamps(m_Cells.size(), 0.f);
    std::vector<float> areas(m_Cells.size(), 0.f);

    auto shine = [this](std::vector<float>& light, bool add, float sourceX, float sourceY,
                        float radius, float brightness) {
        const int reach = (int)std::ceil(radius);
        for (int y = (int)sourceY - reach; y <= (int)sourceY + reach; ++y)
            for (int x = (int)sourceX - reach; x <= (int)sourceX + reach; ++x)
            {
                if (!inside(x, y) || at(x, y).tile == Tile::Wall)
                    continue;

                const float cx = (float)x + 0.5f, cy = (float)y + 0.5f;
                const float distance = std::hypot(cx - sourceX, cy - sourceY);
                if (distance >= radius || !lineOfSight(*this, sourceX, sourceY, cx, cy))
                    continue;

                const float falloff = 1.f - distance / radius;
                float& cell = light[(std::size_t)y * m_Width + x];
                cell = add ? cell + brightness * falloff * falloff
                           : std::max(cell, brightness * falloff * falloff);
            }
    };

    for (const ThingSpawn& spawn: m_Spawns)
        if (spawn.type == ThingType::Lamp)
            shine(lamps, true, spawn.x, spawn.y, LAMP_RADIUS, LAMP_BRIGHTNESS);

    for (int y = 0; y < m_Height; ++y)
        for (int x = 0; x < m_Width; ++x)
        {
            const Cell& cell = at(x, y);
            if (cell.tile != Tile::Empty)
                continue;
            if (cell.outdoors)
                shine(areas, false, (float)x + 0.5f, (float)y + 0.5f, SKY_SPILL_RADIUS, SKY_SPILL);
            if (cell.floor == Floor::Nukage)
                shine(areas, false, (float)x + 0.5f, (float)y + 0.5f, NUKAGE_RADIUS, NUKAGE_GLOW);
        }

    m_Light.resize(m_Cells.size());
    for (std::size_t i = 0; i < m_Cells.size(); ++i)
    {
        float light = ambient + lamps[i] + areas[i];
        if (m_Cells[i].outdoors)
            light = std::max(light, SKY_LIGHT);
        m_Light[i] = std::min(light, 1.25f);
    }
}

} // namespace doom
