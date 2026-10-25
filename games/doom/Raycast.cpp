#include "Raycast.h"
#include <cmath>

namespace doom {

namespace {

/// Where the ray crosses the door across the middle of cell (x, y), if it
/// does so between entering and leaving the cell and the door has not slid
/// out of the way there.
bool hitDoor(const Cell& cell, int x, int y, float ox, float oy, float dx, float dy,
             float tEnter, float tExit, RayHit& hit)
{
    // The door is the line through the middle of the cell, and slides
    // towards the low end of it as it opens, so what is left of it covers
    // `open` to 1 across the cell.
    const float originAcross = cell.doorAlongX ? oy : ox;
    const float directionAcross = cell.doorAlongX ? dy : dx;
    if (directionAcross == 0.f)
        return false;

    const float middle = (float)(cell.doorAlongX ? y : x) + 0.5f;
    const float t = (middle - originAcross) / directionAcross;
    if (t < tEnter || t > tExit)
        return false;

    const float along = cell.doorAlongX ? ox + dx * t - (float)x : oy + dy * t - (float)y;
    if (along < cell.open)
        return false;

    hit.hit = true;
    hit.t = t;
    hit.cellX = x;
    hit.cellY = y;
    hit.side = cell.doorAlongX ? 1 : 0;
    // The texture rides along with the door.
    hit.u = along - cell.open;
    hit.tile = Tile::Door;
    hit.texture = WallTexture::Door;
    return true;
}

} // namespace

RayHit castRay(const Map& map, float ox, float oy, float dx, float dy, float maxT)
{
    RayHit hit;

    int x = (int)std::floor(ox);
    int y = (int)std::floor(oy);

    // How far along the ray one whole cell is, across x and across y.
    const float deltaX = dx == 0.f ? 1e30f : std::fabs(1.f / dx);
    const float deltaY = dy == 0.f ? 1e30f : std::fabs(1.f / dy);

    const int stepX = dx < 0.f ? -1 : 1;
    const int stepY = dy < 0.f ? -1 : 1;
    // How far along the ray the next cell boundary is, across x and y.
    float nextX = dx < 0.f ? (ox - (float)x) * deltaX : ((float)x + 1.f - ox) * deltaX;
    float nextY = dy < 0.f ? (oy - (float)y) * deltaY : ((float)y + 1.f - oy) * deltaY;

    float tEnter = 0.f;
    // -1 while still in the cell the ray started in.
    int side = -1;
    bool cameFromDoor = false;

    // Far more cells than any ray crosses, but a bound all the same.
    for (int steps = 0; steps < 1024 && tEnter <= maxT; ++steps)
    {
        if (!map.inside(x, y))
        {
            // Open edges count as stone, so a ray always ends somewhere.
            hit.hit = true;
            hit.t = tEnter;
            hit.cellX = x;
            hit.cellY = y;
            hit.side = side < 0 ? 0 : side;
            hit.tile = Tile::Wall;
            return hit;
        }

        const Cell& cell = map.at(x, y);
        const float tExit = std::fmin(nextX, nextY);

        if (cell.tile == Tile::Door)
        {
            if (hitDoor(cell, x, y, ox, oy, dx, dy, tEnter, tExit, hit))
            {
                if (hit.t > maxT)
                    hit.hit = false;
                return hit;
            }
        }
        // The cell the ray starts in never stops it, so a ray from inside
        // a wall still gets out.
        else if (cell.tile != Tile::Empty && side >= 0)
        {
            hit.hit = true;
            hit.t = tEnter;
            hit.cellX = x;
            hit.cellY = y;
            hit.side = side;
            hit.tile = cell.tile;
            hit.texture = cameFromDoor ? WallTexture::Jamb : cell.texture;

            const float along = side == 0 ? oy + dy * tEnter : ox + dx * tEnter;
            hit.u = along - std::floor(along);
            // Keep textures reading left to right whichever way the face
            // looks.
            if ((side == 0 && dx < 0.f) || (side == 1 && dy > 0.f))
                hit.u = 1.f - hit.u;
            return hit;
        }

        cameFromDoor = cell.tile == Tile::Door;

        if (nextX < nextY)
        {
            tEnter = nextX;
            nextX += deltaX;
            x += stepX;
            side = 0;
        }
        else
        {
            tEnter = nextY;
            nextY += deltaY;
            y += stepY;
            side = 1;
        }
    }

    hit.t = maxT;
    return hit;
}

bool lineOfSight(const Map& map, float x0, float y0, float x1, float y1)
{
    const RayHit hit = castRay(map, x0, y0, x1 - x0, y1 - y0, 1.f);
    return !hit.hit || hit.t >= 1.f;
}

} // namespace doom
