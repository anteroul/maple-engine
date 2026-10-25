#ifndef MAPLEDOOM_RAYCAST_H
#define MAPLEDOOM_RAYCAST_H

#include "Map.h"

namespace doom {

/// Where a ray first meets something solid.
struct RayHit {
    /// False if the ray reached its limit first.
    bool hit = false;
    /// How far along the ray the hit is, in multiples of the direction it
    /// was cast in: with a unit direction, the distance. Cast along the
    /// camera's direction plus some of its plane, it is the distance from
    /// the camera plane, which is what keeps walls from bulging like a fish
    /// eye.
    float t = 0.f;
    int cellX = 0;
    int cellY = 0;
    /// 0 for a face looking west or east, 1 for one looking north or south.
    int side = 0;
    /// Across the face from 0 to 1, left to right as seen from the ray.
    float u = 0.f;
    Tile tile = Tile::Empty;
    WallTexture texture = WallTexture::Stone;
};

/// Walk the grid from (x, y) along (dx, dy), cell by cell (a DDA), until
/// something solid is in the way or `maxT` is passed.
///
/// Doors stand across the middle of their cell, so the ray goes half way
/// in before meeting one, and slips past whatever part of it has slid open.
/// The walls either side of a door, seen from inside the door's cell, are
/// door jambs.
RayHit castRay(const Map& map, float x, float y, float dx, float dy, float maxT = 128.f);

/// Whether nothing solid lies between the two points.
bool lineOfSight(const Map& map, float x0, float y0, float x1, float y1);

} // namespace doom

#endif //MAPLEDOOM_RAYCAST_H
