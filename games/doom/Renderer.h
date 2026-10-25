#ifndef MAPLEDOOM_RENDERER_H
#define MAPLEDOOM_RENDERER_H

#include "Art.h"
#include "Game.h"
#include "Rendering/Image.h"
#include <array>
#include <cstdint>
#include <vector>

namespace doom {

/// Draws the game the way Wolfenstein 3D and Doom did, on the CPU, one
/// column of pixels at a time.
///
/// For each column of the screen a ray is cast across the map until it meets
/// a wall, and the wall's texture is drawn as tall as it is near. Floors and
/// ceilings are drawn a row at a time, every row of the floor being a line of
/// the map at one distance. Things are billboards, standing upright and
/// facing the camera, and are hidden column by column wherever a wall is
/// nearer. Everything dims with distance and with how dark its part of the
/// map is.
class Renderer {
public:
    static constexpr int WIDTH = 480;
    static constexpr int HEIGHT = 300;
    static constexpr int STATUS_HEIGHT = 34;
    static constexpr int VIEW_HEIGHT = HEIGHT - STATUS_HEIGHT;

    explicit Renderer(const Art& art);

    /// Draw the whole screen into `frame`, which must be WIDTH x HEIGHT.
    /// With `automap` on, the map is drawn over the view.
    void render(const Game& game, Image& frame, bool automap);

private:
    struct Camera {
        float x, y;
        float dirX, dirY;
        /// The camera plane: half the width of the view, at distance one.
        float planeX, planeY;
        /// Eye height, in wall heights.
        float eye;
        float horizon;
        /// Pixels a thing one unit tall spans at distance one.
        float scale;
    };

    Camera makeCamera(const Game& game) const;
    void drawFlats(const Game& game, const Camera& camera, Image& frame);
    void drawWalls(const Game& game, const Camera& camera, Image& frame);
    void drawSprites(const Game& game, const Camera& camera, Image& frame);
    void drawWeapon(const Game& game, Image& frame);
    void drawTints(const Game& game, Image& frame);
    void drawStatusBar(const Game& game, Image& frame);
    void drawMessages(const Game& game, Image& frame);
    void drawAutomap(const Game& game, Image& frame);
    void drawIntermission(const Game& game, Image& frame);

    const Sprite& thingSprite(const Thing& thing) const;
    /// The map's light at a point, blended between the cells round it.
    float lightAt(const Map& map, float x, float y) const;

    const Art& m_Art;
    std::array<float, WIDTH> m_Depth{};
    /// The sky's column for each screen column this frame.
    std::array<int, WIDTH> m_SkyColumn{};
    /// Light added everywhere for a moment by a muzzle flash.
    float m_Flash = 0.f;
    /// Wall cells the player has seen, for the automap.
    std::vector<std::uint8_t> m_Seen;
    int m_Serial = -1;
    Image m_StatusBar;
};

} // namespace doom

#endif //MAPLEDOOM_RENDERER_H
