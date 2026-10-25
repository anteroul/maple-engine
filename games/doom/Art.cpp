#include "Art.h"
#include "Font.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <numbers>
#include "Log/msg.h"

namespace doom {

namespace {

constexpr float PI = std::numbers::pi_v<float>;

// --------------------------------------------------------------- colour --

struct Colour {
    float r, g, b;
};

constexpr Colour operator*(Colour c, float s) { return {c.r * s, c.g * s, c.b * s}; }
constexpr Colour operator+(Colour a, Colour b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
constexpr Colour mix(Colour a, Colour b, float t) { return a * (1.f - t) + b * t; }

std::uint32_t pack(Colour c)
{
    auto channel = [](float v) { return (int)std::lround(std::clamp(v, 0.f, 1.f) * 255.f); };
    return rgba(channel(c.r), channel(c.g), channel(c.b));
}

Colour unpack(std::uint32_t pixel)
{
    return {(float)redOf(pixel) / 255.f, (float)greenOf(pixel) / 255.f, (float)blueOf(pixel) / 255.f};
}

float smoothstep(float edge0, float edge1, float x)
{
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// ---------------------------------------------------------------- noise --

std::uint32_t hash(int x, int y, int seed)
{
    std::uint32_t h = (std::uint32_t)x * 374761393u + (std::uint32_t)y * 668265263u + (std::uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/// The same random number from 0 to 1 every time for the same arguments.
float random01(int x, int y, int seed)
{
    return (float)(hash(x, y, seed) & 0xFFFFFF) / (float)0x1000000;
}

/// Smooth noise from 0 to 1, repeating every `period` units.
float noise(float x, float y, int period, int seed)
{
    const int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    const float fx = x - (float)x0, fy = y - (float)y0;
    auto at = [&](int ix, int iy) {
        return random01(((ix % period) + period) % period, ((iy % period) + period) % period, seed);
    };
    const float sx = fx * fx * (3.f - 2.f * fx), sy = fy * fy * (3.f - 2.f * fy);
    const float top = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * sx;
    const float bottom = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * sx;
    return top + (bottom - top) * sy;
}

/// Octaves of noise over a picture `size` pixels across, `cells` noise cells
/// across at the coarsest. It wraps round at the picture's edges, so
/// textures made from it tile.
float fractal(float px, float py, int size, int cells, int seed, int octaves = 4)
{
    float sum = 0.f, amplitude = 0.5f, total = 0.f;
    for (int octave = 0; octave < octaves; ++octave)
    {
        const int period = cells << octave;
        sum += noise(px / (float)size * (float)period, py / (float)size * (float)period, period,
                     seed + octave * 31) * amplitude;
        total += amplitude;
        amplitude *= 0.5f;
    }
    return sum / total;
}

template<typename ColourAt>
Image paint(int width, int height, ColourAt colourAt)
{
    Image image(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            image.at(x, y) = pack(colourAt(x, y));
    return image;
}

// ------------------------------------------------------------------ walls --

Image stoneWall()
{
    constexpr int SIZE = 64;
    constexpr int COURSE = 16;

    // Four courses of stones, each cut into stones of random width.
    std::array<std::vector<int>, SIZE / COURSE> cuts;
    for (int row = 0; row < (int)cuts.size(); ++row)
    {
        const int offset = (row * 23) % SIZE;
        int position = 0;
        for (int stone = 0; SIZE - position >= 10; ++stone)
        {
            cuts[row].push_back((offset + position) % SIZE);
            position += 14 + (int)(random01(row, stone, 7) * 14.f);
        }
        std::sort(cuts[row].begin(), cuts[row].end());
    }

    return paint(SIZE, SIZE, [&](int x, int y) {
        const int row = y / COURSE, ly = y % COURSE;
        const std::vector<int>& cut = cuts[row];

        // The stone x is on: the last cut at or before it, wrapping round.
        int index = (int)cut.size() - 1;
        int start = cut.back() - SIZE;
        for (int i = 0; i < (int)cut.size(); ++i)
            if (cut[i] <= x)
            {
                index = i;
                start = cut[i];
            }
        const int end = index + 1 < (int)cut.size() ? cut[index + 1] : cut[0] + SIZE;
        const int lx = x - start, width = end - start;

        const float grain = fractal((float)x, (float)y, SIZE, 8, 11);
        Colour stone = Colour{0.45f, 0.43f, 0.40f} * (0.8f + 0.32f * random01(row, index, 3)) * (0.78f + 0.42f * grain);
        stone = mix(stone, Colour{0.36f, 0.33f, 0.27f}, random01(row, index, 5) * 0.6f);

        if (lx == 0 || ly == 0)
            return Colour{0.12f, 0.11f, 0.1f} * (0.8f + 0.4f * grain);
        if (lx == 1 || ly == 1)
            return stone * 1.25f;
        if (lx == width - 1 || ly == COURSE - 1)
            return stone * 0.65f;
        return stone;
    });
}

Image brickWall()
{
    return paint(64, 64, [](int x, int y) {
        const int row = y / 8, ly = y % 8;
        const int shifted = (x + (row % 2) * 8) % 64;
        const int brick = shifted / 16, lx = shifted % 16;
        const float grain = fractal((float)x, (float)y, 64, 16, 23, 3);
        const Colour colour = Colour{0.56f, 0.24f, 0.16f} * (0.78f + 0.36f * random01(brick, row, 21))
                              * (0.8f + 0.38f * grain);

        if (ly == 7 || lx == 15)
            return Colour{0.22f, 0.2f, 0.18f} * (0.8f + 0.4f * grain);
        if (ly == 0 || lx == 0)
            return colour * 1.15f;
        if (ly == 6 || lx == 14)
            return colour * 0.75f;
        return colour;
    });
}

Image techWall()
{
    return paint(64, 64, [](int x, int y) {
        const float grain = fractal((float)x, (float)y, 64, 8, 31);
        const Colour metal = Colour{0.34f, 0.37f, 0.42f} * (0.85f + 0.3f * grain);
        const int px = x % 32;

        // Panel seams, lit from above.
        if (px == 0 || y == 0 || y == 44)
            return Colour{0.1f, 0.11f, 0.13f};
        if (px == 1 || y == 1 || y == 45)
            return metal * 1.3f;
        if (px == 31 || y == 63 || y == 43)
            return metal * 0.6f;

        // A strip light across the panels.
        if (y >= 16 && y <= 22)
        {
            if (y == 16 || y == 22)
                return Colour{0.12f, 0.13f, 0.15f};
            if (px >= 6 && px <= 25)
            {
                const float glow = 1.f - (float)std::abs(y - 19) / 4.f;
                return mix(Colour{0.2f, 0.5f, 0.6f}, Colour{0.75f, 1.f, 1.f}, glow);
            }
            return Colour{0.16f, 0.17f, 0.2f};
        }

        // Rivets, each with a shadow.
        for (int ry: {5, 39, 49, 59})
            for (int rx: {4, 27})
            {
                if (px == rx && y == ry)
                    return Colour{0.75f, 0.78f, 0.82f};
                if (px == rx + 1 && y == ry + 1)
                    return metal * 0.5f;
            }

        // Vents.
        if (y >= 50 && y <= 58 && px >= 9 && px <= 22 && y % 3 == 0)
            return Colour{0.08f, 0.08f, 0.1f};

        // Grime gathers towards the floor.
        return metal * (1.f - (y > 44 ? 0.18f * (float)(y - 44) / 20.f : 0.f));
    });
}

Image woodWall()
{
    return paint(64, 64, [](int x, int y) {
        const int plank = x / 16, lx = x % 16;
        const float wobble = fractal((float)x, (float)y, 64, 4, 41 + plank, 3);
        const float grain = 0.5f + 0.5f * std::sin(((float)lx + wobble * 10.f) * 1.3f + (float)plank * 2.f);
        const Colour wood = mix(Colour{0.34f, 0.2f, 0.1f}, Colour{0.52f, 0.33f, 0.17f}, grain)
                            * (0.82f + 0.3f * random01(plank, 0, 43));

        if (lx == 0)
            return Colour{0.1f, 0.06f, 0.03f};
        if (lx == 1)
            return wood * 1.2f;
        if (lx == 15)
            return wood * 0.7f;
        if ((lx == 7 || lx == 8) && (y == 6 || y == 7 || y == 56 || y == 57))
            return Colour{0.6f, 0.58f, 0.55f};
        // Planks butt together at a different height each.
        if (y == 12 + (int)(random01(plank, 1, 47) * 40.f))
            return Colour{0.12f, 0.07f, 0.03f};
        return wood;
    });
}

Image hellWall()
{
    constexpr int CELLS = 4;
    constexpr float CELL = 64.f / CELLS;

    // Rock split into plates, with fire glowing in the cracks between them:
    // the cracks are where two plates' centres are nearly equally close.
    auto centre = [](int cx, int cy, float& x, float& y) {
        const int wx = (cx % CELLS + CELLS) % CELLS, wy = (cy % CELLS + CELLS) % CELLS;
        x = (float)cx * CELL + 3.f + random01(wx, wy, 51) * (CELL - 6.f);
        y = (float)cy * CELL + 3.f + random01(wx, wy, 52) * (CELL - 6.f);
    };

    return paint(64, 64, [&](int x, int y) {
        const float px = (float)x + 0.5f, py = (float)y + 0.5f;
        const int cx = x / (int)CELL, cy = y / (int)CELL;
        float nearest = 1e9f, second = 1e9f;
        for (int oy = -1; oy <= 1; ++oy)
            for (int ox = -1; ox <= 1; ++ox)
            {
                float fx, fy;
                centre(cx + ox, cy + oy, fx, fy);
                const float distance = std::hypot(px - fx, py - fy);
                if (distance < nearest)
                {
                    second = nearest;
                    nearest = distance;
                }
                else if (distance < second)
                {
                    second = distance;
                }
            }

        const float edge = second - nearest;
        const float grain = fractal((float)x, (float)y, 64, 8, 53);
        const Colour rock = mix(Colour{0.2f, 0.05f, 0.04f}, Colour{0.46f, 0.12f, 0.08f}, grain);

        if (edge < 1.3f)
            return mix(Colour{1.f, 0.85f, 0.4f}, Colour{0.95f, 0.4f, 0.08f}, edge / 1.3f);
        if (edge < 2.6f)
            return rock * 0.5f;
        // Each plate bulges a little, lit from above.
        return rock * (0.8f + 0.4f * std::clamp(1.f - nearest / CELL, 0.f, 1.f));
    });
}

Image doorTexture()
{
    return paint(64, 64, [](int x, int y) {
        const float grain = fractal((float)x, (float)y, 64, 8, 61);
        const Colour metal = Colour{0.42f, 0.42f, 0.45f} * (0.85f + 0.3f * grain);

        // The frame round the edge.
        if (x < 3 || x > 60 || y < 3 || y > 60)
        {
            if (x == 0 || y == 0)
                return metal * 1.2f;
            if (x == 63 || y == 63)
                return metal * 0.45f;
            return metal * 0.7f;
        }

        // Hazard stripes across the middle.
        if (y >= 28 && y <= 35)
        {
            if (y == 28 || y == 35)
                return Colour{0.08f, 0.08f, 0.08f};
            return (x + y) / 4 % 2 ? Colour{0.88f, 0.72f, 0.1f} * (0.85f + 0.3f * grain)
                                   : Colour{0.1f, 0.1f, 0.1f};
        }

        // Two recessed panels: shadow above and left, light below and right.
        for (auto [top, bottom]: {std::pair{8, 23}, {40, 55}})
            if (x >= 8 && x <= 55 && y >= top && y <= bottom)
            {
                if (y == top || x == 8)
                    return metal * 0.5f;
                if (y == bottom || x == 55)
                    return metal * 1.25f;
                return metal * 0.82f;
            }

        // Rivets down both sides.
        if ((x == 5 || x == 58) && y % 8 == 4)
            return Colour{0.75f, 0.75f, 0.78f};
        return metal;
    });
}

Image jambTexture()
{
    return paint(64, 64, [](int x, int y) {
        const float grain = fractal((float)x, (float)y, 64, 8, 63);
        const Colour metal = Colour{0.4f, 0.4f, 0.43f} * (0.85f + 0.3f * grain);
        // The track the door slides in.
        if (x >= 26 && x <= 37)
        {
            if (x == 26)
                return metal * 0.4f;
            if (x == 37)
                return metal * 1.3f;
            return Colour{0.12f, 0.12f, 0.13f};
        }
        if ((x == 10 || x == 53) && y % 16 == 8)
            return Colour{0.78f, 0.78f, 0.8f};
        return metal;
    });
}

Image exitTexture()
{
    Image image = paint(64, 64, [](int x, int y) {
        const float grain = fractal((float)x, (float)y, 64, 8, 65);
        const Colour panel = Colour{0.24f, 0.25f, 0.28f} * (0.85f + 0.3f * grain);

        // The sign: a black box with a red border.
        if (x >= 12 && x <= 51 && y >= 5 && y <= 18)
        {
            if (x == 12 || x == 51 || y == 5 || y == 18)
                return Colour{0.6f, 0.08f, 0.06f};
            return Colour{0.03f, 0.03f, 0.03f};
        }

        // The switch plate, with a slot for the lever.
        if (x >= 22 && x <= 41 && y >= 27 && y <= 54)
        {
            if (x >= 29 && x <= 34 && y >= 31 && y <= 50)
                return Colour{0.06f, 0.06f, 0.07f};
            if (x == 22 || y == 27)
                return Colour{0.75f, 0.75f, 0.78f};
            if (x == 41 || y == 54)
                return Colour{0.25f, 0.25f, 0.27f};
            return Colour{0.52f, 0.52f, 0.55f} * (0.9f + 0.2f * grain);
        }

        if (x % 32 == 0 || y == 0 || y == 63)
            return panel * 0.4f;
        return panel;
    });

    // The lever, up and red.
    for (int y = 33; y <= 38; ++y)
        for (int x = 27; x <= 36; ++x)
            image.at(x, y) = pack(y == 33 ? Colour{1.f, 0.45f, 0.4f} : Colour{0.8f, 0.1f, 0.08f});

    font::drawText(image, 32 - font::textWidth("EXIT") / 2, 8, "EXIT", rgba(255, 40, 30));
    return image;
}

// ------------------------------------------------------------ flats, sky --

Image stoneFloor()
{
    return paint(64, 64, [](int x, int y) {
        const int tileX = x / 32, tileY = y / 32, lx = x % 32, ly = y % 32;
        const float grain = fractal((float)x, (float)y, 64, 8, 71);
        const Colour stone = Colour{0.36f, 0.33f, 0.29f} * (0.82f + 0.26f * random01(tileX, tileY, 73))
                             * (0.78f + 0.44f * grain);
        if (lx == 0 || ly == 0)
            return Colour{0.13f, 0.12f, 0.11f};
        if (lx == 1 || ly == 1)
            return stone * 1.15f;
        // Cracks.
        if (fractal((float)x, (float)y, 64, 4, 75) > 0.72f)
            return stone * 0.6f;
        return stone;
    });
}

Image metalFloor()
{
    return paint(64, 64, [](int x, int y) {
        const float grain = fractal((float)x, (float)y, 64, 8, 77);
        const Colour metal = Colour{0.38f, 0.38f, 0.4f} * (0.82f + 0.3f * grain);
        if (x % 32 == 0 || y % 32 == 0)
            return Colour{0.15f, 0.15f, 0.16f};

        // Diamond plate: raised ridges, alternating which way they lean.
        const int lx = x % 8, ly = y % 8;
        if ((x / 8 + y / 8) % 2 == 0)
        {
            if (lx == ly && lx >= 2 && lx <= 5)
                return metal * 1.35f;
            if (lx == ly + 1 && lx >= 3 && lx <= 6)
                return metal * 0.6f;
        }
        else
        {
            if (lx + ly == 7 && lx >= 2 && lx <= 5)
                return metal * 1.35f;
            if (lx + ly == 8 && lx >= 3 && lx <= 6)
                return metal * 0.6f;
        }
        return metal;
    });
}

Image dirtFloor()
{
    return paint(64, 64, [](int x, int y) {
        const float g = fractal((float)x, (float)y, 64, 4, 81, 5);
        Colour dirt = mix(Colour{0.24f, 0.18f, 0.11f}, Colour{0.44f, 0.34f, 0.21f}, g);
        if (random01(x, y, 83) > 0.9f)
            dirt = mix(dirt, Colour{0.25f, 0.34f, 0.12f}, 0.7f);
        if (fractal((float)x, (float)y, 64, 16, 85, 2) > 0.74f)
            dirt = Colour{0.5f, 0.48f, 0.44f} * (0.7f + 0.5f * g);
        return dirt;
    });
}

Image nukageTexture()
{
    return paint(64, 64, [](int x, int y) {
        const float g = fractal((float)x, (float)y, 64, 4, 87);
        const float swirl = 0.5f + 0.5f * std::sin(g * 16.f);
        return mix(Colour{0.12f, 0.45f, 0.06f}, Colour{0.6f, 1.f, 0.3f}, swirl * swirl);
    });
}

Image stoneCeiling()
{
    return paint(64, 64, [](int x, int y) {
        const float g = fractal((float)x, (float)y, 64, 8, 89);
        if (x % 32 == 0 || y % 32 == 0)
            return Colour{0.12f, 0.12f, 0.12f};
        return Colour{0.27f, 0.26f, 0.25f} * (0.72f + 0.5f * g);
    });
}

Image metalCeiling()
{
    return paint(64, 64, [](int x, int y) {
        const float g = fractal((float)x, (float)y, 64, 8, 91);
        const int lx = x % 32, ly = y % 32;
        if (lx == 0 || ly == 0)
            return Colour{0.1f, 0.1f, 0.11f};
        // A light fitting in every panel.
        if (lx >= 9 && lx <= 22 && ly >= 12 && ly <= 19)
        {
            if (lx == 9 || lx == 22 || ly == 12 || ly == 19)
                return Colour{0.45f, 0.45f, 0.47f};
            return Colour{1.f, 0.98f, 0.88f};
        }
        return Colour{0.3f, 0.31f, 0.33f} * (0.8f + 0.35f * g);
    });
}

Image skyTexture()
{
    constexpr int WIDTH = 512, HEIGHT = 128;
    return paint(WIDTH, HEIGHT, [](int x, int y) {
        const float t = (float)y / (float)(HEIGHT - 1);

        // A blood red sky, burning orange towards the horizon.
        Colour sky = mix(Colour{0.1f, 0.02f, 0.03f}, Colour{0.48f, 0.08f, 0.05f}, smoothstep(0.f, 0.6f, t));
        sky = mix(sky, Colour{0.98f, 0.48f, 0.14f}, smoothstep(0.45f, 1.f, t));

        const float cloud = fractal((float)x, (float)y * 2.5f, WIDTH, 8, 93, 5);
        sky = mix(sky, Colour{0.7f, 0.22f, 0.1f}, smoothstep(0.52f, 0.78f, cloud) * (1.f - t) * 0.8f);

        // Mountains along the bottom.
        const float ridge = 16.f + 40.f * fractal((float)x, 0.f, WIDTH, 6, 95, 4);
        const float depth = (float)(HEIGHT - y) - ridge;
        if (depth < 0.f)
        {
            if (depth > -1.5f)
                return Colour{0.55f, 0.16f, 0.08f};
            return Colour{0.13f, 0.04f, 0.04f} * (0.7f + 0.6f * fractal((float)x, (float)y, WIDTH, 32, 97));
        }
        return sky;
    });
}

// --------------------------------------------------------------- canvas --

/// How much light falls on a rounded surface whose normal leans by
/// (nx, ny) towards the edges: lit from the upper left, in front.
float lighting(float nx, float ny, float roundness)
{
    const float nz = std::sqrt(std::max(0.f, 1.f - nx * nx - ny * ny));
    const float light = std::max(0.f, -0.45f * nx - 0.55f * ny + 0.7f * nz);
    return 1.f + roundness * (0.85f * light - 0.65f);
}

/// Somewhere to draw sprites, out of shaded shapes. Pixels are either
/// solid or see through.
class Canvas {
public:
    Canvas(int width, int height) : image(width, height, 0) {}

    Image image;

    void put(int x, int y, Colour colour)
    {
        if (x >= 0 && y >= 0 && x < image.width && y < image.height)
            image.at(x, y) = pack(colour);
    }

    /// An ellipse shaded as though it bulged towards the viewer, more so
    /// the rounder it is.
    void ellipse(float cx, float cy, float rx, float ry, Colour colour, float roundness = 0.7f)
    {
        for (int y = (int)std::floor(cy - ry); y <= (int)std::ceil(cy + ry); ++y)
            for (int x = (int)std::floor(cx - rx); x <= (int)std::ceil(cx + rx); ++x)
            {
                const float nx = ((float)x + 0.5f - cx) / rx, ny = ((float)y + 0.5f - cy) / ry;
                if (nx * nx + ny * ny <= 1.f)
                    put(x, y, colour * lighting(nx, ny, roundness));
            }
    }

    /// A tapered capsule from (x0, y0) to (x1, y1), radius r0 to r1, shaded
    /// like a cylinder: arms, legs, barrels.
    void limb(float x0, float y0, float x1, float y1, float r0, float r1, Colour colour, float roundness = 0.8f)
    {
        const float reach = std::max(r0, r1);
        const float abx = x1 - x0, aby = y1 - y0;
        const float length2 = std::max(abx * abx + aby * aby, 1e-4f);

        for (int y = (int)std::floor(std::min(y0, y1) - reach); y <= (int)std::ceil(std::max(y0, y1) + reach); ++y)
            for (int x = (int)std::floor(std::min(x0, x1) - reach); x <= (int)std::ceil(std::max(x0, x1) + reach); ++x)
            {
                const float px = (float)x + 0.5f, py = (float)y + 0.5f;
                const float t = std::clamp(((px - x0) * abx + (py - y0) * aby) / length2, 0.f, 1.f);
                const float radius = r0 + (r1 - r0) * t;
                const float dx = (px - (x0 + abx * t)) / radius, dy = (py - (y0 + aby * t)) / radius;
                if (dx * dx + dy * dy <= 1.f)
                    put(x, y, colour * lighting(dx, dy, roundness));
            }
    }

    /// A filled polygon, shaded from `topShade` at its top to `bottomShade`
    /// at its bottom.
    void polygon(const std::vector<std::pair<float, float>>& points, Colour colour,
                 float topShade = 1.f, float bottomShade = 1.f)
    {
        float top = 1e9f, bottom = -1e9f, left = 1e9f, right = -1e9f;
        for (auto [x, y]: points)
        {
            top = std::min(top, y);
            bottom = std::max(bottom, y);
            left = std::min(left, x);
            right = std::max(right, x);
        }

        for (int y = (int)std::floor(top); y <= (int)std::ceil(bottom); ++y)
            for (int x = (int)std::floor(left); x <= (int)std::ceil(right); ++x)
            {
                const float px = (float)x + 0.5f, py = (float)y + 0.5f;
                bool inside = false;
                for (std::size_t i = 0, j = points.size() - 1; i < points.size(); j = i++)
                {
                    const auto [xi, yi] = points[i];
                    const auto [xj, yj] = points[j];
                    if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi)
                        inside = !inside;
                }
                if (inside)
                {
                    const float t = bottom > top ? (py - top) / (bottom - top) : 0.f;
                    put(x, y, colour * (topShade + (bottomShade - topShade) * t));
                }
            }
    }

    void rect(float x0, float y0, float x1, float y1, Colour colour, float topShade = 1.f, float bottomShade = 1.f)
    {
        polygon({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, colour, topShade, bottomShade);
    }

    /// An upright cylinder seen from the side: barrels, columns, poles.
    void cylinder(float x0, float y0, float x1, float y1, Colour colour)
    {
        const float cx = (x0 + x1) / 2.f, rx = (x1 - x0) / 2.f;
        for (int y = (int)std::floor(y0); y < (int)std::ceil(y1); ++y)
            for (int x = (int)std::floor(x0); x < (int)std::ceil(x1); ++x)
                put(x, y, colour * lighting(std::clamp(((float)x + 0.5f - cx) / rx, -1.f, 1.f), 0.f, 0.9f));
    }

    /// Darken the solid pixels at the shape's edge, so it stands out from
    /// whatever is behind it. With `edgesOpen` false, the canvas's own edges
    /// do not count, for shapes cut off by them, like an arm coming up from
    /// the bottom of the screen.
    void outline(float darken = 0.45f, bool edgesOpen = true)
    {
        const Image before = image;
        auto clear = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= image.width || y >= image.height)
                return edgesOpen;
            return alphaOf(before.at(x, y)) < 128;
        };
        for (int y = 0; y < image.height; ++y)
            for (int x = 0; x < image.width; ++x)
                if (!clear(x, y) && (clear(x - 1, y) || clear(x + 1, y) || clear(x, y - 1) || clear(x, y + 1)))
                    image.at(x, y) = pack(unpack(before.at(x, y)) * darken);
    }

    /// Blend every solid pixel towards `colour`.
    void tint(Colour colour, float amount)
    {
        for (std::uint32_t& pixel: image.pixels)
            if (alphaOf(pixel) >= 128)
                pixel = pack(mix(unpack(pixel), colour, amount));
    }

    /// Draw `source` over this canvas, solid pixels only, turned by `angle`
    /// (clockwise) about its point (sourceX, sourceY), which lands on
    /// (x, y) here.
    void stamp(const Image& source, float sourceX, float sourceY, float x, float y, float angle)
    {
        const float c = std::cos(angle), s = std::sin(angle);
        for (int py = 0; py < image.height; ++py)
            for (int px = 0; px < image.width; ++px)
            {
                const float dx = (float)px + 0.5f - x, dy = (float)py + 0.5f - y;
                const int sx = (int)std::floor(sourceX + dx * c + dy * s);
                const int sy = (int)std::floor(sourceY - dx * s + dy * c);
                if (sx < 0 || sy < 0 || sx >= source.width || sy >= source.height)
                    continue;
                const std::uint32_t pixel = source.at(sx, sy);
                if (alphaOf(pixel) >= 128)
                    image.at(px, py) = pixel;
            }
    }
};

/// A spiky flash of light, for muzzle flashes and blasts.
void starburst(Canvas& canvas, float cx, float cy, float rx, float ry, int points, float spin, Colour outer)
{
    std::vector<std::pair<float, float>> star;
    for (int i = 0; i < points * 2; ++i)
    {
        const float angle = spin + (float)i * PI / (float)points;
        const float reach = i % 2 ? 0.45f : 1.f;
        star.push_back({cx + std::cos(angle) * rx * reach, cy + std::sin(angle) * ry * reach});
    }
    canvas.polygon(star, outer);
    canvas.ellipse(cx, cy, rx * 0.5f, ry * 0.5f, Colour{1.f, 0.88f, 0.45f}, 0.f);
    canvas.ellipse(cx, cy, rx * 0.25f, ry * 0.25f, Colour{1.f, 1.f, 0.9f}, 0.f);
}

// ------------------------------------------------------------- monsters --

/// What a monster is doing, for drawing it.
struct Pose {
    /// Through a stride, in radians.
    float walk = 0.f;
    /// 0 not attacking, 1 winding up, 2 striking.
    int attack = 0;
    bool pain = false;
};

constexpr Colour BLOOD{0.55f, 0.03f, 0.03f};

void drawTrooper(Canvas& c, const Pose& pose)
{
    const float cx = (float)c.image.width / 2.f;
    const float top = std::fabs(std::sin(pose.walk)) * 1.2f;
    const Colour skin{0.62f, 0.56f, 0.48f}, armour{0.33f, 0.4f, 0.2f}, pants{0.3f, 0.28f, 0.18f};
    const Colour boots{0.16f, 0.13f, 0.1f}, gun{0.2f, 0.2f, 0.22f}, hair{0.22f, 0.17f, 0.12f};

    const float liftLeft = std::max(0.f, std::sin(pose.walk)) * 4.f;
    const float liftRight = std::max(0.f, -std::sin(pose.walk)) * 4.f;
    c.limb(cx - 5.f, 38.f + top, cx - 6.f, 59.f - liftLeft, 4.f, 3.5f, pants);
    c.limb(cx + 5.f, 38.f + top, cx + 6.f, 59.f - liftRight, 4.f, 3.5f, pants);
    c.ellipse(cx - 6.f, 61.f - liftLeft, 4.5f, 2.5f, boots);
    c.ellipse(cx + 6.f, 61.f - liftRight, 4.5f, 2.5f, boots);

    c.ellipse(cx, 29.f + top, 10.f, 11.f, armour);
    c.ellipse(cx, 27.f + top, 6.f, 6.f, armour * 1.12f, 0.4f);
    c.rect(cx - 9.f, 36.f + top, cx + 9.f, 39.f + top, Colour{0.32f, 0.2f, 0.1f});

    const float headX = cx + (pose.pain ? 2.5f : 0.f);
    c.ellipse(headX, 13.f + top, 6.f, 7.f, skin);
    c.ellipse(headX, 8.5f + top, 6.2f, 3.6f, hair, 0.3f);
    for (float side: {-2.5f, 2.5f})
    {
        c.put((int)(headX + side - 0.5f), (int)(13.f + top), Colour{0.9f, 0.1f, 0.08f});
        c.put((int)(headX + side + 0.5f), (int)(13.f + top), Colour{0.9f, 0.1f, 0.08f});
    }
    c.rect(headX - 2.f, 17.f + top, headX + 2.f, 18.f + top, Colour{0.25f, 0.12f, 0.1f});

    if (pose.pain)
    {
        // Arms flung up, the rifle in one hand.
        c.limb(cx - 10.f, 21.f + top, cx - 16.f, 11.f + top, 3.f, 2.6f, armour);
        c.ellipse(cx - 16.f, 10.f + top, 2.6f, 2.6f, skin);
        c.limb(cx + 12.f, 4.f + top, cx + 18.f, 24.f + top, 1.8f, 1.8f, gun, 0.3f);
        c.limb(cx + 10.f, 21.f + top, cx + 15.f, 13.f + top, 3.f, 2.6f, armour);
        c.ellipse(cx + 15.f, 12.f + top, 2.6f, 2.6f, skin);
    }
    else if (pose.attack > 0)
    {
        // Aiming at the viewer: the rifle seen end on.
        c.limb(cx - 10.f, 21.f + top, cx - 3.5f, 29.f + top, 3.f, 2.6f, armour);
        c.limb(cx + 10.f, 21.f + top, cx + 3.5f, 31.f + top, 3.f, 2.6f, armour);
        c.rect(cx - 3.5f, 23.f + top, cx + 3.5f, 31.f + top, gun, 1.3f, 0.8f);
        c.ellipse(cx, 26.5f + top, 2.f, 2.f, Colour{0.02f, 0.02f, 0.02f}, 0.f);
        c.ellipse(cx - 3.5f, 29.f + top, 2.6f, 2.6f, skin);
        c.ellipse(cx + 3.5f, 31.f + top, 2.6f, 2.6f, skin);
        if (pose.attack == 2)
            starburst(c, cx, 26.5f + top, 8.f, 7.f, 6, 0.3f, Colour{1.f, 0.6f, 0.15f});
    }
    else
    {
        // The rifle held across the chest, swinging a little with each step.
        const float swing = std::sin(pose.walk) * 2.f;
        c.limb(cx - 9.f, 24.f + top + swing, cx + 11.f, 37.f + top - swing, 1.8f, 1.8f, gun, 0.3f);
        c.limb(cx - 10.f, 21.f + top, cx - 5.f, 28.f + top + swing, 3.f, 2.6f, armour);
        c.ellipse(cx - 5.f, 28.f + top + swing, 2.6f, 2.6f, skin);
        c.limb(cx + 10.f, 21.f + top, cx + 7.f, 34.f + top - swing, 3.f, 2.6f, armour);
        c.ellipse(cx + 7.f, 34.f + top - swing, 2.6f, 2.6f, skin);
    }
}

/// Imps and knights: hunched, clawed and spiked, throwing balls of fire.
struct FiendStyle {
    Colour skin;
    Colour belly;
    Colour legs;
    Colour spikes;
    Colour eyes;
    Colour ball;
    bool horns;
};

void drawFiend(Canvas& c, const Pose& pose, const FiendStyle& style)
{
    // Designed 64 pixels tall, and scaled to fit the canvas.
    const float k = (float)c.image.height / 64.f;
    const float cx = (float)c.image.width / 2.f;
    const float top = std::fabs(std::sin(pose.walk)) * 1.2f;
    auto X = [&](float x) { return cx + x * k; };
    auto Y = [&](float y) { return (y + top) * k; };
    auto R = [&](float r) { return r * k; };

    // Legs bent backwards like a beast's.
    const float liftLeft = std::max(0.f, std::sin(pose.walk)) * 4.f;
    const float liftRight = std::max(0.f, -std::sin(pose.walk)) * 4.f;
    for (auto [side, lift]: {std::pair{-1.f, liftLeft}, {1.f, liftRight}})
    {
        const float knee = 49.f - lift * 0.5f, ankle = 60.f - lift;
        c.limb(X(side * 5.f), Y(37.f), X(side * 8.f), (knee) * k, R(4.5f), R(3.5f), style.legs);
        c.limb(X(side * 8.f), (knee) * k, X(side * 6.f), (ankle) * k, R(3.f), R(2.2f), style.legs * 0.9f);
        c.ellipse(X(side * 6.5f), (ankle + 1.5f) * k, R(4.f), R(2.f), style.legs * 0.6f);
    }

    c.ellipse(X(0.f), Y(29.f), R(10.5f), R(12.f), style.skin);
    c.ellipse(X(0.f), Y(32.f), R(6.f), R(7.f), style.belly, 0.5f);
    // Ribs, and spikes down the chest.
    for (float rib: {27.f, 30.f, 33.f})
        c.rect(X(-5.f), Y(rib), X(5.f), Y(rib + 0.6f), style.belly * 0.7f);
    for (auto [sx, sy]: {std::pair{-6.f, 23.f}, {6.f, 23.f}, {-8.f, 30.f}, {8.f, 30.f}})
        c.polygon({{X(sx - 1.2f), Y(sy)}, {X(sx + 1.2f), Y(sy)}, {X(sx * 1.25f), Y(sy - 3.f)}}, style.spikes);

    // Spikes off the shoulders.
    for (float side: {-1.f, 1.f})
        c.polygon({{X(side * 6.f), Y(20.f)}, {X(side * 12.f), Y(21.f)}, {X(side * 13.f), Y(11.f)}}, style.spikes, 1.2f, 0.8f);

    const float headX = pose.pain ? 2.f : 0.f;
    c.ellipse(X(headX), Y(13.f), R(6.5f), R(6.5f), style.skin);
    c.rect(X(headX - 5.5f), Y(10.5f), X(headX + 5.5f), Y(12.f), style.skin * 0.55f);
    for (float side: {-2.6f, 2.6f})
        c.ellipse(X(headX + side), Y(13.5f), R(1.5f), R(1.2f), style.eyes, 0.f);
    c.rect(X(headX - 3.f), Y(16.5f), X(headX + 3.f), Y(18.5f), Colour{0.15f, 0.03f, 0.02f});
    for (float tooth: {-2.f, 0.f, 2.f})
        c.put((int)X(headX + tooth), (int)Y(16.8f), Colour{0.95f, 0.92f, 0.8f});

    if (style.horns)
    {
        for (float side: {-1.f, 1.f})
        {
            c.limb(X(headX + side * 4.f), Y(8.f), X(headX + side * 9.f), Y(4.f), R(2.f), R(1.4f), style.spikes);
            c.limb(X(headX + side * 9.f), Y(4.f), X(headX + side * 9.f), Y(-2.f), R(1.4f), R(0.6f), style.spikes);
        }
    }
    else
    {
        for (float side: {-3.f, 0.f, 3.f})
            c.polygon({{X(headX + side - 1.5f), Y(8.f)}, {X(headX + side + 1.5f), Y(8.f)}, {X(headX + side), Y(4.f)}},
                      style.spikes);
    }

    auto arm = [&](float side, float elbowX, float elbowY, float handX, float handY) {
        c.limb(X(side * 10.f), Y(21.f), X(elbowX), Y(elbowY), R(3.4f), R(2.8f), style.skin);
        c.limb(X(elbowX), Y(elbowY), X(handX), Y(handY), R(2.8f), R(2.4f), style.skin);
        c.ellipse(X(handX), Y(handY), R(2.8f), R(2.8f), style.skin * 0.9f);
        for (float claw: {-1.6f, 0.f, 1.6f})
            c.put((int)X(handX + claw), (int)Y(handY + 2.8f), style.spikes);
        // A spike off each elbow.
        c.polygon({{X(elbowX - 1.5f), Y(elbowY)}, {X(elbowX + 1.5f), Y(elbowY)},
                   {X(elbowX + side * 4.f), Y(elbowY - 2.f)}}, style.spikes);
    };

    if (pose.pain)
    {
        arm(-1.f, -15.f, 14.f, -17.f, 6.f);
        arm(1.f, 15.f, 14.f, 17.f, 6.f);
    }
    else if (pose.attack == 1)
    {
        // Winding up, a ball of fire held high.
        arm(-1.f, -13.f, 31.f, -12.f, 39.f);
        arm(1.f, 15.f, 13.f, 12.f, 4.f);
        c.ellipse(X(12.f), Y(1.f), R(4.5f), R(4.5f), style.ball, 0.f);
        c.ellipse(X(12.f), Y(1.f), R(2.5f), R(2.5f), mix(style.ball, Colour{1.f, 1.f, 0.9f}, 0.6f), 0.f);
    }
    else if (pose.attack == 2)
    {
        // Throwing: the arm thrust out towards the viewer.
        arm(-1.f, -13.f, 31.f, -12.f, 39.f);
        arm(1.f, 9.f, 25.f, 4.f, 26.f);
    }
    else
    {
        const float swing = std::sin(pose.walk) * 2.5f;
        arm(-1.f, -13.f, 30.f, -12.f, 39.f + swing);
        arm(1.f, 13.f, 30.f, 12.f, 39.f - swing);
    }
}

void drawDemon(Canvas& c, const Pose& pose)
{
    const float cx = (float)c.image.width / 2.f;
    const float top = std::fabs(std::sin(pose.walk)) * 1.5f;
    const Colour pink{0.82f, 0.42f, 0.48f}, dark{0.55f, 0.22f, 0.28f}, belly{0.92f, 0.62f, 0.62f};
    const Colour horn{0.88f, 0.84f, 0.72f};

    const float liftLeft = std::max(0.f, std::sin(pose.walk)) * 4.f;
    const float liftRight = std::max(0.f, -std::sin(pose.walk)) * 4.f;
    for (auto [side, lift]: {std::pair{-1.f, liftLeft}, {1.f, liftRight}})
    {
        c.limb(cx + side * 12.f, 38.f + top, cx + side * 14.f, 50.f - lift, 6.f, 5.f, dark);
        c.ellipse(cx + side * 14.f, 52.f - lift, 5.f, 3.f, Colour{0.25f, 0.15f, 0.12f});
    }

    c.ellipse(cx, 30.f + top, 21.f, 15.f, pink);
    c.ellipse(cx, 35.f + top, 12.f, 8.f, belly, 0.5f);

    const float swing = std::sin(pose.walk) * 3.f;
    for (float side: {-1.f, 1.f})
    {
        const float reach = pose.attack == 2 ? 6.f : 0.f;
        c.limb(cx + side * 18.f, 27.f + top, cx + side * (23.f - reach), 42.f + top + side * swing, 4.5f, 3.5f, pink);
        c.ellipse(cx + side * (23.f - reach), 44.f + top + side * swing, 3.5f, 3.f, dark);
    }

    const float headX = cx + (pose.pain ? 2.f : 0.f);
    c.ellipse(headX, 22.f + top, 13.f, 10.f, pink * 0.95f);
    for (float side: {-1.f, 1.f})
    {
        c.limb(headX + side * 8.f, 15.f + top, headX + side * 13.f, 9.f + top, 2.f, 1.f, horn);
        c.rect(headX + side * 2.5f - 2.5f, 15.f + top, headX + side * 2.5f + 2.5f + side * 2.f, 16.5f + top, dark * 0.6f);
        c.ellipse(headX + side * 5.f, 18.f + top, 2.f, 1.5f, Colour{1.f, 0.9f, 0.2f}, 0.f);
    }

    // The jaws: wider open the harder it bites.
    const float open = pose.attack == 0 ? (pose.pain ? 0.6f : 0.15f) : (pose.attack == 1 ? 0.6f : 1.f);
    const float jaw = 2.f + 6.f * open;
    const float mouthY = 27.f + top;
    c.ellipse(headX, mouthY, 9.f, jaw, Colour{0.32f, 0.02f, 0.04f}, 0.f);
    for (int i = -4; i <= 4; ++i)
    {
        const float x = headX + (float)i * 2.f;
        const float spread = 1.f - (float)(i * i) / 25.f;
        c.polygon({{x - 0.9f, mouthY - jaw * 0.85f * spread}, {x + 0.9f, mouthY - jaw * 0.85f * spread},
                   {x, mouthY - jaw * 0.85f * spread + 2.4f}}, horn);
        c.polygon({{x - 0.9f, mouthY + jaw * 0.85f * spread}, {x + 0.9f, mouthY + jaw * 0.85f * spread},
                   {x, mouthY + jaw * 0.85f * spread - 2.4f}}, horn);
    }
}

/// Draw the poses of one kind of monster, and tip it over for its death.
template<typename Draw>
MonsterArt monsterArt(int width, int height, float worldHeight, Draw draw)
{
    auto make = [&](Pose pose) {
        Canvas canvas(width, height);
        draw(canvas, pose);
        canvas.outline();
        return canvas.image;
    };

    MonsterArt art;
    for (int i = 0; i < 4; ++i)
        art.walk[i] = {make(Pose{(float)i * PI / 2.f}), worldHeight};
    art.attack[0] = {make(Pose{0.f, 1}), worldHeight};
    art.attack[1] = {make(Pose{0.f, 2}), worldHeight};

    const Image hurt = make(Pose{0.f, 0, true});
    Canvas pain(width, height);
    pain.image = hurt;
    pain.tint(Colour{0.9f, 0.1f, 0.05f}, 0.25f);
    art.pain = {pain.image, worldHeight};

    // Topple sideways about the feet, bleeding more as it goes. The frames
    // are wider, to fit the body lying down, but just as tall, so it stays
    // the same size.
    const std::array<float, 5> angles = {0.3f, 0.75f, 1.15f, 1.45f, 1.57f};
    const int wide = height + 8;
    for (int frame = 0; frame < 5; ++frame)
    {
        Canvas canvas(wide, height);
        const float angle = angles[frame];
        const float footX = (float)wide / 2.f - (float)height * 0.45f * std::sin(angle);
        if (frame > 0)
            canvas.ellipse((float)wide / 2.f, (float)height - 3.f, 4.f + (float)frame * 4.f,
                           1.5f + (float)frame * 0.6f, BLOOD, 0.2f);
        canvas.stamp(hurt, (float)width / 2.f, (float)height - 1.f, footX, (float)height - 1.f - (float)frame * 0.6f, angle);
        for (int drop = 0; drop < frame * 3; ++drop)
            canvas.ellipse((float)wide / 2.f + (random01(frame, drop, 101) - 0.5f) * (float)height * 0.8f,
                           (float)height - 2.f - random01(frame, drop, 103) * (float)height * 0.4f, 1.f, 1.f, BLOOD, 0.f);
        art.death[frame] = {canvas.image, worldHeight};
    }
    return art;
}

// ------------------------------------------------------- things, effects --

Sprite barrelSprite()
{
    Canvas c(24, 32);
    const Colour metal{0.32f, 0.38f, 0.3f};
    c.cylinder(3.f, 6.f, 21.f, 31.f, metal);
    for (int band: {10, 25})
        c.cylinder(3.f, (float)band, 21.f, (float)band + 2.f, metal * 0.55f);
    // Rust.
    for (int y = 6; y < 31; ++y)
        for (int x = 3; x < 21; ++x)
            if (fractal((float)x, (float)y, 32, 8, 105) > 0.66f)
                c.put(x, y, Colour{0.4f, 0.24f, 0.12f});
    c.ellipse(12.f, 6.f, 9.f, 3.f, metal * 0.5f, 0.2f);
    c.ellipse(12.f, 6.f, 7.f, 2.f, Colour{0.5f, 1.f, 0.3f}, 0.f);
    // Slime running down the side.
    for (int x: {7, 10, 15})
        c.rect((float)x, 6.f, (float)x + 1.f, 9.f + (float)(x % 4) * 2.f, Colour{0.4f, 0.85f, 0.2f});
    c.outline();
    return {c.image, 0.34f};
}

Sprite lampSprite()
{
    Canvas c(16, 60);
    c.cylinder(7.f, 14.f, 10.f, 56.f, Colour{0.3f, 0.3f, 0.32f});
    c.ellipse(8.5f, 57.f, 6.f, 2.5f, Colour{0.28f, 0.28f, 0.3f});
    c.rect(4.f, 56.f, 13.f, 59.f, Colour{0.22f, 0.22f, 0.24f});
    c.ellipse(8.5f, 9.f, 6.f, 7.f, Colour{1.f, 0.92f, 0.65f}, 0.25f);
    c.ellipse(8.5f, 9.f, 3.f, 4.f, Colour{1.f, 1.f, 0.95f}, 0.f);
    for (int x: {5, 8, 12})
        c.rect((float)x, 3.f, (float)x + 1.f, 15.f, Colour{0.25f, 0.22f, 0.2f});
    c.rect(4.f, 15.f, 13.f, 16.f, Colour{0.25f, 0.22f, 0.2f});
    c.outline(0.6f);
    return {c.image, 0.62f, true};
}

Sprite columnSprite()
{
    Canvas c(20, 56);
    const Colour marble{0.62f, 0.6f, 0.58f};
    c.cylinder(4.f, 8.f, 16.f, 48.f, marble);
    for (int x = 6; x < 16; x += 3)
        c.rect((float)x, 9.f, (float)x + 1.f, 47.f, marble * 0.7f);
    c.cylinder(2.f, 3.f, 18.f, 8.f, marble * 1.05f);
    c.cylinder(2.f, 48.f, 18.f, 55.f, marble * 1.05f);
    for (int y = 8; y < 48; ++y)
        for (int x = 4; x < 16; ++x)
            if (fractal((float)x, (float)y, 64, 16, 107) > 0.68f)
                c.put(x, y, marble * 0.55f);
    c.outline();
    return {c.image, 0.6f};
}

Sprite boxSprite(int width, int height, float worldHeight, Colour colour, Colour crossColour)
{
    Canvas c(width, height);
    const float w = (float)width, h = (float)height;
    c.rect(1.f, 3.f, w - 1.f, h, colour, 1.f, 0.75f);
    c.polygon({{1.f, 3.f}, {w - 1.f, 3.f}, {w - 4.f, 0.f}, {4.f, 0.f}}, colour * 1.15f);
    const float cx = w / 2.f, cy = (h + 3.f) / 2.f, arm = h * 0.28f, thick = h * 0.12f;
    c.rect(cx - thick, cy - arm, cx + thick, cy + arm, crossColour);
    c.rect(cx - arm, cy - thick, cx + arm, cy + thick, crossColour);
    c.outline();
    return {c.image, worldHeight};
}

Sprite armorSprite()
{
    Canvas c(28, 24);
    const Colour green{0.25f, 0.62f, 0.25f};
    c.polygon({{4.f, 2.f}, {10.f, 2.f}, {14.f, 7.f}, {18.f, 2.f}, {24.f, 2.f}, {26.f, 10.f}, {24.f, 23.f},
               {4.f, 23.f}, {2.f, 10.f}}, green, 1.2f, 0.75f);
    c.polygon({{10.f, 2.f}, {14.f, 7.f}, {18.f, 2.f}, {16.f, 9.f}, {12.f, 9.f}}, green * 0.45f);
    c.rect(13.5f, 9.f, 14.5f, 22.f, green * 0.6f);
    for (int y: {12, 17})
        c.rect(5.f, (float)y, 23.f, (float)y + 1.f, green * 0.7f);
    c.outline();
    return {c.image, 0.22f};
}

Sprite clipSprite()
{
    Canvas c(10, 12);
    c.rect(1.f, 3.f, 9.f, 12.f, Colour{0.26f, 0.26f, 0.24f}, 1.2f, 0.8f);
    c.rect(2.f, 0.f, 8.f, 3.f, Colour{0.85f, 0.7f, 0.3f}, 1.2f, 0.8f);
    c.outline();
    return {c.image, 0.1f};
}

Sprite shellsSprite()
{
    Canvas c(20, 12);
    c.rect(1.f, 4.f, 19.f, 12.f, Colour{0.65f, 0.12f, 0.08f}, 1.1f, 0.75f);
    for (int i = 0; i < 4; ++i)
    {
        const float x = 3.f + (float)i * 4.f;
        c.rect(x, 0.f, x + 3.f, 4.f, Colour{0.75f, 0.15f, 0.1f});
        c.rect(x, 3.f, x + 3.f, 5.f, Colour{0.9f, 0.75f, 0.35f});
    }
    c.outline();
    return {c.image, 0.11f};
}

Sprite rocketBoxSprite()
{
    Canvas c(22, 14);
    c.rect(1.f, 5.f, 21.f, 14.f, Colour{0.45f, 0.32f, 0.18f}, 1.1f, 0.75f);
    c.rect(1.f, 9.f, 21.f, 10.f, Colour{0.3f, 0.2f, 0.1f});
    for (float y: {2.5f, 5.5f})
    {
        c.limb(3.f, y, 17.f, y, 1.8f, 1.8f, Colour{0.55f, 0.56f, 0.58f});
        c.ellipse(18.5f, y, 2.f, 1.6f, Colour{0.8f, 0.15f, 0.1f});
    }
    c.outline();
    return {c.image, 0.13f};
}

Sprite shotgunPickup()
{
    Canvas c(44, 12);
    const Colour steel{0.25f, 0.26f, 0.28f}, wood{0.45f, 0.28f, 0.14f};
    c.limb(2.f, 5.f, 31.f, 5.f, 1.6f, 1.6f, steel);
    c.limb(4.f, 7.5f, 24.f, 7.5f, 1.2f, 1.2f, steel * 0.8f);
    c.rect(12.f, 5.f, 21.f, 9.f, wood);
    c.polygon({{29.f, 3.f}, {43.f, 5.f}, {43.f, 11.f}, {31.f, 8.f}}, wood, 1.2f, 0.8f);
    c.outline();
    return {c.image, 0.08f};
}

Sprite chaingunPickup()
{
    Canvas c(44, 16);
    const Colour steel{0.25f, 0.26f, 0.28f};
    for (float y: {5.f, 8.f, 11.f})
        c.limb(2.f, y, 26.f, y, 1.4f, 1.4f, steel * (1.2f - (y - 5.f) * 0.06f));
    c.rect(24.f, 3.f, 40.f, 13.f, steel * 0.8f, 1.2f, 0.7f);
    c.rect(32.f, 12.f, 36.f, 16.f, steel * 0.6f);
    c.outline();
    return {c.image, 0.12f};
}

Sprite launcherPickup()
{
    Canvas c(48, 16);
    const Colour olive{0.3f, 0.35f, 0.22f};
    c.limb(4.f, 8.f, 44.f, 8.f, 5.f, 5.f, olive);
    c.ellipse(4.f, 8.f, 2.5f, 4.5f, Colour{0.05f, 0.05f, 0.04f}, 0.f);
    c.rect(20.f, 3.f, 22.f, 13.f, olive * 0.6f);
    c.rect(30.f, 12.f, 34.f, 16.f, Colour{0.2f, 0.2f, 0.2f});
    c.outline();
    return {c.image, 0.12f};
}

/// A ball of glowing light, with a white hot middle.
Sprite glowBall(int size, float worldHeight, float radius, Colour colour, int seed)
{
    Canvas c(size, size);
    const float cx = (float)size / 2.f;
    for (int i = 0; i < 6; ++i)
    {
        const float angle = random01(i, 0, seed) * 2.f * PI;
        const float reach = radius * 0.4f * random01(i, 1, seed);
        c.ellipse(cx + std::cos(angle) * reach, cx + std::sin(angle) * reach, radius * 0.6f, radius * 0.6f,
                  colour * 0.85f, 0.f);
    }
    c.ellipse(cx, cx, radius * 0.75f, radius * 0.75f, colour, 0.f);
    c.ellipse(cx, cx, radius * 0.45f, radius * 0.45f, mix(colour, Colour{1.f, 1.f, 0.9f}, 0.6f), 0.f);
    c.ellipse(cx, cx, radius * 0.2f, radius * 0.2f, Colour{1.f, 1.f, 1.f}, 0.f);
    return {c.image, worldHeight, true};
}

/// Fragments flying out from a burst, `t` from 0 to 1 through it.
Sprite burstSprite(int size, float worldHeight, float t, Colour colour, int seed)
{
    Canvas c(size, size);
    const float cx = (float)size / 2.f;
    const float reach = (float)size * (0.15f + 0.3f * t);
    const float blob = (float)size * (0.18f - 0.1f * t);
    if (t < 0.5f)
        c.ellipse(cx, cx, reach * 0.8f, reach * 0.8f, colour * 0.8f, 0.f);
    for (int i = 0; i < 8; ++i)
    {
        const float angle = (float)i * PI / 4.f + random01(i, seed, 109) * 0.5f;
        c.ellipse(cx + std::cos(angle) * reach, cx + std::sin(angle) * reach, blob, blob,
                  mix(colour, Colour{1.f, 1.f, 0.85f}, 0.5f * (1.f - t)), 0.f);
    }
    if (t < 0.4f)
        c.ellipse(cx, cx, reach * 0.4f, reach * 0.4f, Colour{1.f, 1.f, 0.9f}, 0.f);
    return {c.image, worldHeight, true};
}

Sprite explosionSprite(int frame)
{
    Canvas c(64, 64);
    const Colour core{1.f, 0.95f, 0.6f}, fire{1.f, 0.55f, 0.12f}, deep{0.85f, 0.25f, 0.06f};
    const Colour smoke{0.3f, 0.26f, 0.23f};

    auto blobs = [&](float spread, float size, Colour colour, int count, int seed) {
        for (int i = 0; i < count; ++i)
        {
            const float angle = random01(i, frame, seed) * 2.f * PI;
            const float reach = spread * random01(i, frame + 7, seed);
            c.ellipse(32.f + std::cos(angle) * reach, 34.f + std::sin(angle) * reach * 0.8f,
                      size * (0.6f + 0.4f * random01(i, 3, seed)), size * (0.6f + 0.4f * random01(i, 4, seed)),
                      colour, 0.2f);
        }
    };

    switch (frame)
    {
        case 0:
            blobs(6.f, 8.f, fire, 6, 111);
            c.ellipse(32.f, 34.f, 9.f, 9.f, core, 0.f);
            break;
        case 1:
            blobs(14.f, 12.f, deep, 8, 113);
            blobs(10.f, 10.f, fire, 8, 115);
            c.ellipse(32.f, 34.f, 10.f, 9.f, core, 0.f);
            break;
        case 2:
            blobs(20.f, 11.f, deep, 10, 117);
            blobs(14.f, 9.f, fire, 8, 119);
            blobs(14.f, 6.f, smoke, 5, 121);
            break;
        default:
            blobs(22.f, 10.f, smoke, 10, 123);
            blobs(16.f, 4.f, fire, 6, 125);
            // Thin the smoke out to see through.
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x)
                    if ((x + y) % 2)
                        c.image.at(x, y) = 0;
            break;
    }
    return {c.image, 0.75f, true};
}

std::array<Sprite, 3> puffSprites()
{
    std::array<Sprite, 3> frames;
    for (int frame = 0; frame < 3; ++frame)
    {
        Canvas c(16, 16);
        const float radius = 2.5f + (float)frame * 1.6f;
        c.ellipse(8.f, 8.f - (float)frame, radius, radius, Colour{0.7f, 0.68f, 0.65f} * (1.f - 0.2f * (float)frame), 0.5f);
        if (frame == 0)
            starburst(c, 8.f, 8.f, 5.f, 5.f, 4, 0.4f, Colour{1.f, 0.8f, 0.3f});
        if (frame == 2)
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                    if ((x + y) % 2)
                        c.image.at(x, y) = 0;
        frames[frame] = {c.image, 0.14f, frame == 0};
    }
    return frames;
}

std::array<Sprite, 3> bloodSprites()
{
    std::array<Sprite, 3> frames;
    for (int frame = 0; frame < 3; ++frame)
    {
        Canvas c(16, 16);
        for (int drop = 0; drop < 6; ++drop)
        {
            const float angle = random01(drop, 0, 127) * 2.f * PI;
            const float reach = 1.f + (float)frame * 2.2f * random01(drop, 1, 127);
            c.ellipse(8.f + std::cos(angle) * reach, 8.f + std::sin(angle) * reach + (float)frame,
                      1.8f - (float)frame * 0.3f, 1.8f - (float)frame * 0.3f, Colour{0.75f, 0.05f, 0.05f}, 0.4f);
        }
        frames[frame] = {c.image, 0.14f};
    }
    return frames;
}

// --------------------------------------------------------------- weapons --

constexpr Colour SKIN{0.8f, 0.58f, 0.44f};

Image drawFist(int frame)
{
    Canvas c(140, 100);
    constexpr std::array<float, 3> x = {100.f, 82.f, 68.f}, y = {80.f, 58.f, 42.f}, scale = {1.f, 0.85f, 0.7f};
    const float fx = x[frame], fy = y[frame], s = scale[frame];

    c.limb(150.f, 130.f, fx + 8.f * s, fy + 6.f * s, 22.f * s, 18.f * s, SKIN * 0.95f);
    c.ellipse(fx, fy, 21.f * s, 17.f * s, SKIN);
    for (int knuckle = 0; knuckle < 4; ++knuckle)
        c.ellipse(fx - 13.f * s + 8.5f * s * (float)knuckle, fy - 11.f * s, 5.f * s, 4.5f * s, SKIN * 1.05f);
    c.limb(fx - 15.f * s, fy + 3.f * s, fx + 5.f * s, fy + 9.f * s, 4.5f * s, 4.f * s, SKIN * 0.95f);
    c.outline(0.5f, false);
    return c.image;
}

Image drawPistol(float recoil)
{
    Canvas c(72, 88);
    const float dy = -recoil;
    const Colour steel{0.3f, 0.31f, 0.34f};

    c.limb(48.f, 110.f + dy, 40.f, 68.f + dy, 16.f, 12.f, SKIN * 0.95f);
    c.polygon({{26.f, 52.f + dy}, {46.f, 52.f + dy}, {42.f, 14.f + dy}, {30.f, 14.f + dy}}, steel, 1.35f, 0.75f);
    c.polygon({{33.f, 14.f + dy}, {39.f, 14.f + dy}, {41.f, 50.f + dy}, {31.f, 50.f + dy}}, steel * 1.45f, 1.2f, 0.8f);
    c.ellipse(36.f, 15.f + dy, 3.5f, 2.f, Colour{0.02f, 0.02f, 0.02f}, 0.f);
    c.rect(35.f, 10.f + dy, 37.f, 14.f + dy, steel * 0.6f);

    // The hand round the grip: palm, thumb up the left, fingers on the right.
    c.ellipse(40.f, 64.f + dy, 13.f, 11.f, SKIN);
    c.limb(29.f, 70.f + dy, 28.f, 51.f + dy, 4.5f, 4.f, SKIN * 1.05f);
    for (int finger = 0; finger < 3; ++finger)
        c.ellipse(51.f, 55.f + dy + (float)finger * 6.f, 4.5f, 3.5f, SKIN * 0.95f);
    c.outline(0.5f, false);
    return c.image;
}

Image drawShotgun(float recoil, float pump)
{
    Canvas c(120, 104);
    const float dy = -recoil;
    const Colour steel{0.22f, 0.23f, 0.25f}, wood{0.45f, 0.28f, 0.14f};

    c.limb(100.f, 130.f + dy, 78.f, 94.f + dy, 18.f, 15.f, SKIN * 0.95f);
    c.polygon({{46.f, 104.f + dy}, {76.f, 104.f + dy}, {70.f, 82.f + dy}, {52.f, 82.f + dy}}, wood, 1.2f, 0.8f);

    // The barrel, and the magazine tube under it, narrowing into the
    // distance.
    c.polygon({{46.f, 104.f + dy}, {74.f, 104.f + dy}, {66.f, 16.f + dy}, {54.f, 16.f + dy}}, steel, 1.5f, 0.8f);
    c.polygon({{57.f, 16.f + dy}, {61.f, 16.f + dy}, {64.f, 100.f + dy}, {55.f, 100.f + dy}}, steel * 1.8f, 1.3f, 0.8f);
    c.rect(53.f, 30.f + dy, 54.5f, 100.f + dy, steel * 0.5f);
    c.rect(65.5f, 30.f + dy, 67.f, 100.f + dy, steel * 0.5f);
    c.ellipse(60.f, 17.f + dy, 4.5f, 2.5f, Colour{0.02f, 0.02f, 0.02f}, 0.f);
    c.ellipse(60.f, 12.f + dy, 1.8f, 1.8f, Colour{0.85f, 0.85f, 0.85f});

    // The pump, and the hand working it.
    const float py = pump + dy;
    c.polygon({{40.f, 80.f + py}, {80.f, 80.f + py}, {75.f, 54.f + py}, {45.f, 54.f + py}}, wood, 1.25f, 0.8f);
    for (float groove = 58.f; groove < 78.f; groove += 4.f)
        c.rect(44.f, groove + py, 76.f, groove + 1.f + py, wood * 0.6f);
    c.limb(14.f, 130.f + py, 38.f, 74.f + py, 17.f, 13.f, SKIN * 0.95f);
    c.ellipse(41.f, 68.f + py, 10.f, 12.f, SKIN);
    for (int finger = 0; finger < 3; ++finger)
        c.ellipse(78.f, 60.f + py + (float)finger * 6.f, 4.f, 3.2f, SKIN * 0.95f);
    c.outline(0.5f, false);
    return c.image;
}

Image drawChaingun(int spin)
{
    Canvas c(130, 96);
    const Colour steel{0.25f, 0.25f, 0.27f};

    c.limb(10.f, 120.f, 34.f, 80.f, 18.f, 14.f, SKIN * 0.95f);
    c.limb(120.f, 120.f, 96.f, 80.f, 18.f, 14.f, SKIN * 0.95f);
    c.polygon({{38.f, 96.f}, {92.f, 96.f}, {82.f, 40.f}, {48.f, 40.f}}, steel, 1.3f, 0.7f);
    c.rect(28.f, 68.f, 44.f, 84.f, steel * 0.6f);
    c.rect(86.f, 68.f, 102.f, 84.f, steel * 0.6f);

    // Three barrels: the lit one moves round as they spin.
    for (int barrel = 0; barrel < 3; ++barrel)
    {
        const float x = 55.f + (float)barrel * 10.f;
        const float lit = (barrel + spin) % 3 == 1 ? 1.5f : 1.f;
        c.polygon({{x - 4.f, 44.f}, {x + 4.f, 44.f}, {x + 3.f, 14.f}, {x - 3.f, 14.f}}, steel * lit, 1.4f, 0.8f);
    }
    c.ellipse(65.f, 14.f, 14.f, 5.f, steel * 1.2f, 0.5f);
    for (int hole = 0; hole < 3; ++hole)
    {
        const float angle = (float)spin * PI / 3.f + (float)hole * 2.f * PI / 3.f;
        c.ellipse(65.f + std::cos(angle) * 8.f, 14.f + std::sin(angle) * 2.5f, 2.5f, 1.4f,
                  Colour{0.02f, 0.02f, 0.02f}, 0.f);
    }
    c.ellipse(34.f, 82.f, 10.f, 9.f, SKIN);
    c.ellipse(96.f, 82.f, 10.f, 9.f, SKIN);
    c.outline(0.5f, false);
    return c.image;
}

Image drawLauncher(float recoil)
{
    Canvas c(130, 100);
    const float dy = -recoil;
    const Colour olive{0.3f, 0.35f, 0.22f};

    c.limb(14.f, 125.f + dy, 40.f, 84.f + dy, 18.f, 14.f, SKIN * 0.95f);
    c.limb(116.f, 125.f + dy, 92.f, 86.f + dy, 18.f, 14.f, SKIN * 0.95f);
    c.polygon({{36.f, 100.f + dy}, {94.f, 100.f + dy}, {82.f, 22.f + dy}, {48.f, 22.f + dy}}, olive, 1.35f, 0.7f);
    c.polygon({{58.f, 22.f + dy}, {66.f, 22.f + dy}, {70.f, 100.f + dy}, {56.f, 100.f + dy}}, olive * 1.35f, 1.2f, 0.75f);
    for (float band: {44.f, 72.f})
        c.rect(40.f, band + dy, 90.f, band + 4.f + dy, olive * 0.55f);
    c.rect(88.f, 40.f + dy, 97.f, 62.f + dy, Colour{0.15f, 0.15f, 0.15f});
    c.ellipse(65.f, 22.f + dy, 17.f, 6.f, olive * 0.6f, 0.3f);
    c.ellipse(65.f, 22.f + dy, 12.f, 4.f, Colour{0.03f, 0.03f, 0.02f}, 0.f);
    c.ellipse(40.f, 86.f + dy, 10.f, 9.f, SKIN);
    c.ellipse(92.f, 88.f + dy, 10.f, 9.f, SKIN);
    c.outline(0.5f, false);
    return c.image;
}

Image muzzleFlash(int width, int height, int seed)
{
    Canvas c(width, height);
    starburst(c, (float)width / 2.f, (float)height / 2.f, (float)width / 2.f - 1.f, (float)height / 2.f - 1.f,
              7, random01(seed, 0, 131) * PI, Colour{1.f, 0.6f, 0.15f});
    return c.image;
}

} // namespace

Art createArt()
{
    Art art;

    art.walls[(int)WallTexture::Stone] = stoneWall();
    art.walls[(int)WallTexture::Brick] = brickWall();
    art.walls[(int)WallTexture::Tech] = techWall();
    art.walls[(int)WallTexture::Wood] = woodWall();
    art.walls[(int)WallTexture::Hell] = hellWall();
    art.walls[(int)WallTexture::Door] = doorTexture();
    art.walls[(int)WallTexture::Exit] = exitTexture();
    art.walls[(int)WallTexture::Jamb] = jambTexture();

    art.floorStone = stoneFloor();
    art.floorMetal = metalFloor();
    art.floorDirt = dirtFloor();
    art.nukage = nukageTexture();
    art.ceilingStone = stoneCeiling();
    art.ceilingMetal = metalCeiling();
    art.sky = skyTexture();

    art.monsters[(int)ThingType::Trooper] = monsterArt(40, 64, 0.64f, drawTrooper);
    art.monsters[(int)ThingType::Imp] = monsterArt(44, 64, 0.64f, [](Canvas& c, const Pose& pose) {
        drawFiend(c, pose, {{0.56f, 0.28f, 0.16f}, {0.68f, 0.42f, 0.28f}, {0.46f, 0.23f, 0.13f},
                            {0.88f, 0.82f, 0.7f}, {1.f, 0.75f, 0.2f}, {1.f, 0.55f, 0.1f}, false});
    });
    art.monsters[(int)ThingType::Demon] = monsterArt(64, 56, 0.56f, drawDemon);
    art.monsters[(int)ThingType::Knight] = monsterArt(60, 88, 0.88f, [](Canvas& c, const Pose& pose) {
        drawFiend(c, pose, {{0.62f, 0.52f, 0.38f}, {0.72f, 0.62f, 0.46f}, {0.38f, 0.25f, 0.15f},
                            {0.9f, 0.86f, 0.74f}, {0.4f, 1.f, 0.3f}, {0.4f, 1.f, 0.4f}, true});
    });

    art.things[(int)ThingType::Barrel] = barrelSprite();
    art.things[(int)ThingType::Lamp] = lampSprite();
    art.things[(int)ThingType::Column] = columnSprite();
    art.things[(int)ThingType::Medkit] = boxSprite(24, 16, 0.15f, Colour{0.9f, 0.9f, 0.88f}, Colour{0.85f, 0.1f, 0.1f});
    art.things[(int)ThingType::Stimpack] = boxSprite(12, 12, 0.1f, Colour{0.9f, 0.9f, 0.88f}, Colour{0.85f, 0.1f, 0.1f});
    art.things[(int)ThingType::Armor] = armorSprite();
    art.things[(int)ThingType::Clip] = clipSprite();
    art.things[(int)ThingType::Shells] = shellsSprite();
    art.things[(int)ThingType::RocketBox] = rocketBoxSprite();
    art.things[(int)ThingType::Shotgun] = shotgunPickup();
    art.things[(int)ThingType::Chaingun] = chaingunPickup();
    art.things[(int)ThingType::RocketLauncher] = launcherPickup();

    art.puff = puffSprites();
    art.blood = bloodSprites();
    for (int frame = 0; frame < 4; ++frame)
        art.explosion[frame] = explosionSprite(frame);
    for (int frame = 0; frame < 2; ++frame)
    {
        art.fireball[frame] = glowBall(18, 0.18f, 7.f - (float)frame, Colour{1.f, 0.45f, 0.08f}, 133 + frame);
        art.plasma[frame] = glowBall(16, 0.16f, 6.f - (float)frame * 0.7f, Colour{0.35f, 1.f, 0.4f}, 135 + frame);
    }
    for (int frame = 0; frame < 3; ++frame)
    {
        art.fireballBurst[frame] = burstSprite(32, 0.32f, (float)frame / 2.f, Colour{1.f, 0.45f, 0.08f}, frame);
        art.plasmaBurst[frame] = burstSprite(32, 0.32f, (float)frame / 2.f, Colour{0.35f, 1.f, 0.4f}, frame + 5);
    }
    art.rocket = glowBall(14, 0.14f, 5.f, Colour{1.f, 0.6f, 0.2f}, 137);
    {
        Canvas nose(14, 14);
        nose.image = art.rocket.image;
        nose.ellipse(7.f, 7.f, 2.5f, 2.5f, Colour{0.55f, 0.55f, 0.58f});
        art.rocket.image = nose.image;
    }

    WeaponArt& fist = art.weapons[(int)Weapon::Fist];
    for (int frame = 0; frame < 3; ++frame)
        fist.frames.push_back(drawFist(frame));

    WeaponArt& pistol = art.weapons[(int)Weapon::Pistol];
    pistol.frames = {drawPistol(0.f), drawPistol(6.f)};
    pistol.flash = muzzleFlash(28, 24, 1);
    pistol.flashX = 36;
    pistol.flashY = 6;

    WeaponArt& shotgun = art.weapons[(int)Weapon::Shotgun];
    shotgun.frames = {drawShotgun(0.f, 0.f), drawShotgun(8.f, 0.f), drawShotgun(-10.f, 10.f), drawShotgun(-14.f, 18.f)};
    shotgun.flash = muzzleFlash(40, 32, 2);
    shotgun.flashX = 60;
    shotgun.flashY = 4;

    WeaponArt& chaingun = art.weapons[(int)Weapon::Chaingun];
    chaingun.frames = {drawChaingun(0), drawChaingun(1)};
    chaingun.flash = muzzleFlash(36, 30, 3);
    chaingun.flashX = 65;
    chaingun.flashY = 4;

    WeaponArt& launcher = art.weapons[(int)Weapon::RocketLauncher];
    launcher.frames = {drawLauncher(0.f), drawLauncher(8.f)};
    launcher.flash = muzzleFlash(44, 36, 4);
    launcher.flashX = 65;
    launcher.flashY = 10;

    return art;
}

std::vector<std::pair<std::string, Image*>> artFiles(Art& art)
{
    std::vector<std::pair<std::string, Image*>> files;

    const char* walls[] = {"wall_stone", "wall_brick", "wall_tech", "wall_wood", "wall_hell", "door", "exit", "door_jamb"};
    for (int i = 0; i < (int)WallTexture::Count; ++i)
        files.push_back({walls[i], &art.walls[i]});

    files.push_back({"floor_stone", &art.floorStone});
    files.push_back({"floor_metal", &art.floorMetal});
    files.push_back({"floor_dirt", &art.floorDirt});
    files.push_back({"nukage", &art.nukage});
    files.push_back({"ceiling_stone", &art.ceilingStone});
    files.push_back({"ceiling_metal", &art.ceilingMetal});
    files.push_back({"sky", &art.sky});

    auto frames = [&](const std::string& name, auto& sprites) {
        for (int i = 0; i < (int)sprites.size(); ++i)
            files.push_back({name + std::to_string(i), &sprites[i].image});
    };

    const char* monsters[] = {"trooper", "imp", "demon", "knight"};
    for (int i = 0; i < 4; ++i)
    {
        MonsterArt& monster = art.monsters[i];
        const std::string name = monsters[i];
        frames(name + "_walk", monster.walk);
        frames(name + "_attack", monster.attack);
        files.push_back({name + "_pain", &monster.pain.image});
        frames(name + "_death", monster.death);
    }

    const char* things[] = {"barrel", "lamp", "column", "medikit", "stimpack", "armor", "clip", "shells",
                            "rockets", "shotgun_pickup", "chaingun_pickup", "launcher_pickup"};
    for (int i = (int)ThingType::Barrel; i < (int)ThingType::Count; ++i)
        files.push_back({things[i - (int)ThingType::Barrel], &art.things[i].image});

    frames("puff", art.puff);
    frames("blood", art.blood);
    frames("explosion", art.explosion);
    frames("fireball", art.fireball);
    frames("fireball_burst", art.fireballBurst);
    frames("plasma", art.plasma);
    frames("plasma_burst", art.plasmaBurst);
    files.push_back({"rocket", &art.rocket.image});

    const char* weapons[] = {"fist", "pistol", "shotgun", "chaingun", "launcher"};
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        WeaponArt& weapon = art.weapons[i];
        for (int frame = 0; frame < (int)weapon.frames.size(); ++frame)
            files.push_back({std::string(weapons[i]) + std::to_string(frame), &weapon.frames[frame]});
        if (!weapon.flash.empty())
            files.push_back({std::string(weapons[i]) + "_flash", &weapon.flash});
    }

    return files;
}

int loadArtOverrides(Art& art, const std::string& directory)
{
    int replaced = 0;
    for (auto& [name, image]: artFiles(art))
    {
        const std::filesystem::path path = std::filesystem::path(directory) / (name + ".png");
        if (!std::filesystem::exists(path))
            continue;
        if (std::optional<Image> loaded = Image::load(path.string()))
        {
            *image = std::move(*loaded);
            ++replaced;
        }
    }
    return replaced;
}

int saveArt(Art& art, const std::string& directory)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error)
    {
        printf("%s \bWARNING: Could not create %s: %s\n", WARNING, directory.c_str(), error.message().c_str());
        return 0;
    }

    int saved = 0;
    for (auto& [name, image]: artFiles(art))
        saved += image->save((std::filesystem::path(directory) / (name + ".png")).string());
    return saved;
}

} // namespace doom
