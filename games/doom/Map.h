#ifndef MAPLEDOOM_MAP_H
#define MAPLEDOOM_MAP_H

#include <cstdint>
#include <string>
#include <vector>

namespace doom {

/// What fills a cell of the map.
enum class Tile : std::uint8_t {
    Empty,
    Wall,
    /// Slides open sideways when used, and closes again by itself.
    Door,
    /// A wall with the exit switch on it: using it ends the level.
    Exit
};

/// How a wall looks. Doors, the exit switch and door jambs have their own.
enum class WallTexture : std::uint8_t {
    Stone, Brick, Tech, Wood, Hell, Door, Exit, Jamb,
    Count
};

/// What is underfoot, and so what is overhead.
enum class Floor : std::uint8_t {
    /// Flagstones under a plain ceiling.
    Stone,
    /// Metal plates under light panels.
    Metal,
    /// Dirt, open to the sky.
    Outdoor,
    /// Glowing toxic slime, which hurts to stand in. Under the sky if laid
    /// over an outdoor floor, otherwise under a plain ceiling.
    Nukage
};

enum class ThingType : std::uint8_t {
    // Monsters come first, see isMonster().
    Trooper, Imp, Demon, Knight,
    // Scenery. Barrels explode when shot, lamps light the room.
    Barrel, Lamp, Column,
    // Pickups come last, see isPickup().
    Medkit, Stimpack, Armor, Clip, Shells, RocketBox, Shotgun, Chaingun, RocketLauncher,
    Count
};

constexpr bool isMonster(ThingType type) { return type <= ThingType::Knight; }
constexpr bool isPickup(ThingType type) { return type >= ThingType::Medkit; }

struct Cell {
    Tile tile = Tile::Empty;
    WallTexture texture = WallTexture::Stone;
    Floor floor = Floor::Stone;
    /// Open to the sky rather than roofed over.
    bool outdoors = false;
    /// Doors only: true if the door runs along x, between walls to its west
    /// and east, so it is crossed going north or south.
    bool doorAlongX = true;
    /// Doors only: how far open it is, from 0 shut to 1 fully open. The game
    /// moves this, the raycaster reads it.
    float open = 0.f;
};

/// Something placed on the map when the level starts, at a cell's centre.
struct ThingSpawn {
    ThingType type;
    float x, y;
};

/// A rectangle of cells given another floor, inclusive at both ends.
struct FloorZone {
    int x0, y0, x1, y1;
    Floor floor;
};

/// A level as it is written down.
///
/// The rows run north to south, and every one must be as long as the first.
///   Walls     # stone  B brick  T tech  W wood  H hell  X exit switch
///   Doors     D
///   Floor     .  or a space
///   Player    ^ > v <  the start, facing north, east, south or west
///   Monsters  z trooper  i imp  d demon  K knight
///   Scenery   o barrel  l lamp  | column
///   Pickups   + medikit  h stimpack  a armor  c clip  s shells  r rockets
///             S shotgun  C chaingun  R rocket launcher
struct MapDefinition {
    std::string name;
    std::vector<std::string> rows;
    /// Floors other than stone, laid down in order. Nukage laid over an
    /// outdoor floor stays outdoors.
    std::vector<FloorZone> zones;
    /// Light everywhere before lamps and the sky add theirs, 0 to 1.
    float ambient = 0.4f;
};

/// A level's layout, and the state of its doors.
class Map {
public:
    /// Throws std::invalid_argument if the rows are ragged, hold a letter
    /// that means nothing, or have no player start, or more than one.
    explicit Map(const MapDefinition& definition);

    int width() const { return m_Width; }
    int height() const { return m_Height; }
    const std::string& name() const { return m_Name; }

    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < m_Width && y < m_Height; }
    Cell& at(int x, int y) { return m_Cells[(std::size_t)y * m_Width + x]; }
    const Cell& at(int x, int y) const { return m_Cells[(std::size_t)y * m_Width + x]; }

    /// Whether a body is kept out of the cell: walls, and doors not yet open
    /// far enough to walk through. Everything outside the map is solid.
    bool blocksMovement(int x, int y) const;
    /// Whether nothing can ever pass the cell, open or shut: walls and the
    /// world's edge, not doors.
    bool isWall(int x, int y) const;

    /// How brightly lit the cell is, from 0 dark to 1 and a little over.
    float light(int x, int y) const;

    float startX() const { return m_StartX; }
    float startY() const { return m_StartY; }
    /// Radians: 0 faces east, and the angle grows clockwise, towards south,
    /// since y runs south.
    float startAngle() const { return m_StartAngle; }
    const std::vector<ThingSpawn>& spawns() const { return m_Spawns; }

private:
    void computeLight(float ambient);

    std::string m_Name;
    int m_Width = 0;
    int m_Height = 0;
    std::vector<Cell> m_Cells;
    std::vector<float> m_Light;
    std::vector<ThingSpawn> m_Spawns;
    float m_StartX = 0.f;
    float m_StartY = 0.f;
    float m_StartAngle = 0.f;
};

} // namespace doom

#endif //MAPLEDOOM_MAP_H
