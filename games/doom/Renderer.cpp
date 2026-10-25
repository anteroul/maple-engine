#include "Renderer.h"
#include "Font.h"
#include "Raycast.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace doom {

namespace {

constexpr float PI = std::numbers::pi_v<float>;

/// Half the width of the view at distance one: a field of view of about 74
/// degrees.
constexpr float PLANE = 0.75f;
constexpr float EYE_HEIGHT = 0.45f;
/// Walls are taller than they are wide, for rooms with some height to them.
/// Sprites are measured in the same units: a trooper is 0.64 tall.
constexpr float WALL_HEIGHT = 1.35f;
constexpr float MAX_DISTANCE = 64.f;

/// How much light is left after travelling `distance` through the gloom.
float fog(float distance)
{
    return 1.f / (1.f + distance * distance * 0.008f);
}

/// Scale a pixel's colour by `shade`, where 256 leaves it as it is.
std::uint32_t shadePixel(std::uint32_t pixel, int shade)
{
    const int r = std::min(255, (int)(pixel & 0xFF) * shade >> 8);
    const int g = std::min(255, (int)(pixel >> 8 & 0xFF) * shade >> 8);
    const int b = std::min(255, (int)(pixel >> 16 & 0xFF) * shade >> 8);
    return rgba(r, g, b);
}

int toShade(float light)
{
    return std::clamp((int)(light * 256.f), 0, 512);
}

std::uint32_t blend(std::uint32_t pixel, int r, int g, int b, float amount)
{
    const float keep = 1.f - amount;
    return rgba((int)((float)redOf(pixel) * keep + (float)r * amount),
                (int)((float)greenOf(pixel) * keep + (float)g * amount),
                (int)((float)blueOf(pixel) * keep + (float)b * amount));
}

/// Texel (u, v) of a texture, u and v from 0 to 1, wrapping.
std::uint32_t sample(const Image& texture, float u, float v)
{
    int x = (int)(u * (float)texture.width) % texture.width;
    int y = (int)(v * (float)texture.height) % texture.height;
    if (x < 0) x += texture.width;
    if (y < 0) y += texture.height;
    return texture.at(x, y);
}

/// The weapon frame to show and whether the muzzle flash is lit, by how
/// long ago the last shot was.
int weaponFrame(Weapon weapon, float sinceShot, float time, bool& flash)
{
    flash = false;
    switch (weapon)
    {
        case Weapon::Fist:
            if (sinceShot < 0.08f) return 1;
            if (sinceShot < 0.2f) return 2;
            if (sinceShot < 0.3f) return 1;
            return 0;
        case Weapon::Pistol:
            flash = sinceShot < 0.07f;
            return sinceShot < 0.12f ? 1 : 0;
        case Weapon::Shotgun:
            // Fire, then work the pump.
            flash = sinceShot < 0.07f;
            if (sinceShot < 0.1f) return 1;
            if (sinceShot < 0.25f) return 0;
            if (sinceShot < 0.4f) return 2;
            if (sinceShot < 0.6f) return 3;
            if (sinceShot < 0.75f) return 2;
            return 0;
        case Weapon::Chaingun:
            flash = sinceShot < 0.06f;
            return sinceShot < 0.13f ? (int)(time * 20.f) % 2 : 0;
        case Weapon::RocketLauncher:
            flash = sinceShot < 0.08f;
            return sinceShot < 0.15f ? 1 : 0;
        default:
            return 0;
    }
}

/// A line one pixel wide, clipped to the rectangle [0, width) x [0, height).
void drawLine(Image& image, float x0, float y0, float x1, float y1, std::uint32_t colour, int height)
{
    const int steps = (int)std::ceil(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))) + 1;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float)i / (float)steps;
        const int x = (int)std::floor(x0 + (x1 - x0) * t), y = (int)std::floor(y0 + (y1 - y0) * t);
        if (x >= 0 && y >= 0 && x < image.width && y < height)
            image.at(x, y) = colour;
    }
}

void centredText(Image& image, int y, const std::string& text, std::uint32_t colour, int scale)
{
    font::drawShadowedText(image, (image.width - font::textWidth(text, scale)) / 2, y, text, colour, scale);
}

} // namespace

Renderer::Renderer(const Art& art)
    : m_Art(art), m_StatusBar(WIDTH, STATUS_HEIGHT)
{
    // The status bar's backing: brushed, scratched metal, with a raised
    // edge round each panel.
    constexpr int PANELS[] = {0, 76, 166, 236, 296, 386, WIDTH};
    for (int y = 0; y < STATUS_HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
        {
            std::uint32_t h = (std::uint32_t)x * 374761393u + (std::uint32_t)y * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            const float grain = (float)((h ^ (h >> 16)) & 0xFF) / 255.f;
            float shade = 0.85f + 0.12f * grain + 0.06f * std::sin((float)y * 0.9f + (float)x * 0.05f);

            for (int edge: PANELS)
            {
                if (x == edge) shade = 1.35f;
                if (x == edge - 1) shade = 0.45f;
            }
            if (y == 0) shade = 1.4f;
            if (y == 1) shade = 1.15f;
            if (y == STATUS_HEIGHT - 1) shade = 0.4f;

            const int value = (int)std::clamp(shade * 82.f, 0.f, 255.f);
            m_StatusBar.at(x, y) = rgba(value, value * 96 / 100, value * 90 / 100);
        }
}

void Renderer::render(const Game& game, Image& frame, bool automap)
{
    const Map& map = game.map();
    if (m_Serial != game.levelSerial())
    {
        m_Serial = game.levelSerial();
        m_Seen.assign((std::size_t)map.width() * map.height(), 0);
    }

    const Player& player = game.player();
    m_Flash = player.weapon != Weapon::Fist && player.sinceShot < 0.08f ? 0.3f : 0.f;

    const Camera camera = makeCamera(game);
    drawFlats(game, camera, frame);
    drawWalls(game, camera, frame);
    drawSprites(game, camera, frame);
    drawWeapon(game, frame);
    drawTints(game, frame);
    if (automap)
        drawAutomap(game, frame);
    drawStatusBar(game, frame);
    drawMessages(game, frame);
    if (game.state() != GameState::Playing)
        drawIntermission(game, frame);
}

Renderer::Camera Renderer::makeCamera(const Game& game) const
{
    const Player& player = game.player();
    Camera camera{};
    camera.x = player.x;
    camera.y = player.y;
    camera.dirX = std::cos(player.angle);
    camera.dirY = std::sin(player.angle);
    camera.planeX = -camera.dirY * PLANE;
    camera.planeY = camera.dirX * PLANE;

    // The view bobs twice a stride, and sinks to the floor on death.
    camera.eye = EYE_HEIGHT + player.bob * std::sin(player.bobPhase * 2.f) * 0.012f;
    if (player.dead)
        camera.eye = std::max(0.12f, EYE_HEIGHT - player.deadTime * 0.6f);

    camera.horizon = (float)VIEW_HEIGHT / 2.f;
    camera.scale = ((float)WIDTH / 2.f) / PLANE;
    return camera;
}

float Renderer::lightAt(const Map& map, float x, float y) const
{
    const float lx = x - 0.5f, ly = y - 0.5f;
    const int x0 = (int)std::floor(lx), y0 = (int)std::floor(ly);
    const float fx = lx - (float)x0, fy = ly - (float)y0;
    const float top = map.light(x0, y0) + (map.light(x0 + 1, y0) - map.light(x0, y0)) * fx;
    const float bottom = map.light(x0, y0 + 1) + (map.light(x0 + 1, y0 + 1) - map.light(x0, y0 + 1)) * fx;
    return top + (bottom - top) * fy;
}

void Renderer::drawFlats(const Game& game, const Camera& camera, Image& frame)
{
    const Map& map = game.map();
    const Image& sky = m_Art.sky;
    const float time = game.time();

    for (int x = 0; x < WIDTH; ++x)
    {
        const float cameraX = 2.f * ((float)x + 0.5f) / (float)WIDTH - 1.f;
        const float angle = std::atan2(camera.dirY + camera.planeY * cameraX, camera.dirX + camera.planeX * cameraX);
        // Twice round the sky texture for once round the horizon.
        const float u = angle / (2.f * PI) + 0.5f;
        m_SkyColumn[x] = (int)(u * 2.f * (float)sky.width) % sky.width;
    }

    const float leftX = camera.dirX - camera.planeX, leftY = camera.dirY - camera.planeY;
    const float rightX = camera.dirX + camera.planeX, rightY = camera.dirY + camera.planeY;

    for (int y = 0; y < VIEW_HEIGHT; ++y)
    {
        std::uint32_t* row = &frame.pixels[(std::size_t)y * WIDTH];
        const bool isFloor = (float)y + 0.5f > camera.horizon;
        const float offset = std::fabs((float)y + 0.5f - camera.horizon);
        const float height = isFloor ? camera.eye : WALL_HEIGHT - camera.eye;

        // Every pixel of this row sees the floor (or ceiling) this far away.
        const float distance = height * camera.scale / offset;
        const float dim = fog(distance);
        const int skyRow = std::clamp((int)((float)y / camera.horizon * (float)sky.height), 0, sky.height - 1);

        const float stepX = distance * (rightX - leftX) / (float)WIDTH;
        const float stepY = distance * (rightY - leftY) / (float)WIDTH;
        float wx = camera.x + distance * leftX + stepX * 0.5f;
        float wy = camera.y + distance * leftY + stepY * 0.5f;

        for (int x = 0; x < WIDTH; ++x, wx += stepX, wy += stepY)
        {
            const int cx = (int)std::floor(wx), cy = (int)std::floor(wy);
            if (!map.inside(cx, cy))
            {
                row[x] = rgba(0, 0, 0);
                continue;
            }

            const Cell& cell = map.at(cx, cy);
            const Floor floor = cell.floor;
            if (!isFloor && cell.outdoors)
            {
                row[x] = sky.at(m_SkyColumn[x], skyRow);
                continue;
            }

            float u = wx - (float)cx, v = wy - (float)cy;
            const Image* texture;
            float light = lightAt(map, wx, wy) + m_Flash;
            if (isFloor)
            {
                switch (floor)
                {
                    case Floor::Metal: texture = &m_Art.floorMetal; break;
                    case Floor::Outdoor: texture = &m_Art.floorDirt; break;
                    case Floor::Nukage:
                        texture = &m_Art.nukage;
                        // Slime flows, and glows.
                        u += time * 0.15f;
                        v += std::sin(time * 0.7f + wx) * 0.08f;
                        light = std::max(light, 0.95f);
                        break;
                    default: texture = &m_Art.floorStone; break;
                }
            }
            else
            {
                texture = floor == Floor::Metal ? &m_Art.ceilingMetal : &m_Art.ceilingStone;
            }

            row[x] = shadePixel(sample(*texture, u, v), toShade(light * dim));
        }
    }
}

void Renderer::drawWalls(const Game& game, const Camera& camera, Image& frame)
{
    const Map& map = game.map();

    for (int x = 0; x < WIDTH; ++x)
    {
        const float cameraX = 2.f * ((float)x + 0.5f) / (float)WIDTH - 1.f;
        const float rayX = camera.dirX + camera.planeX * cameraX;
        const float rayY = camera.dirY + camera.planeY * cameraX;

        // Cast along the direction plus some of the camera plane, the ray's
        // t is the distance from the plane, not from the eye, so walls stay
        // flat rather than bulging in the middle.
        const RayHit hit = castRay(map, camera.x, camera.y, rayX, rayY, MAX_DISTANCE);
        if (!hit.hit)
        {
            m_Depth[x] = 1e9f;
            continue;
        }

        const float distance = std::max(hit.t, 0.02f);
        m_Depth[x] = distance;
        if (map.inside(hit.cellX, hit.cellY))
            m_Seen[(std::size_t)hit.cellY * map.width() + hit.cellX] = 1;

        // A wall is lit by the cell in front of it: the one the ray came
        // from. A door stands in its own cell.
        int litX = hit.cellX, litY = hit.cellY;
        if (hit.tile != Tile::Door)
        {
            if (hit.side == 0)
                litX -= rayX > 0.f ? 1 : -1;
            else
                litY -= rayY > 0.f ? 1 : -1;
        }
        // Walls facing north and south a shade darker than those facing east
        // and west, which keeps corners easy to read.
        const float contrast = hit.side == 1 ? 0.8f : 1.f;
        const int shade = toShade((map.light(litX, litY) + m_Flash) * fog(distance) * contrast);

        const float top = camera.horizon - (WALL_HEIGHT - camera.eye) * camera.scale / distance;
        const float bottom = camera.horizon + camera.eye * camera.scale / distance;
        const int y0 = std::max(0, (int)std::ceil(top - 0.5f));
        const int y1 = std::min(VIEW_HEIGHT, (int)std::ceil(bottom - 0.5f));

        const Image& texture = m_Art.walls[(int)hit.texture];
        const int column = std::min(texture.width - 1, (int)(hit.u * (float)texture.width));
        const float step = (float)texture.height / (bottom - top);
        float v = ((float)y0 + 0.5f - top) * step;

        for (int y = y0; y < y1; ++y, v += step)
        {
            const int texel = std::min(texture.height - 1, (int)v);
            frame.pixels[(std::size_t)y * WIDTH + x] = shadePixel(texture.at(column, texel), shade);
        }
    }
}

const Sprite& Renderer::thingSprite(const Thing& thing) const
{
    if (!isMonster(thing.type))
        return m_Art.things[(int)thing.type];

    const MonsterArt& art = m_Art.monsters[(int)thing.type];
    switch (thing.state)
    {
        case ThingState::Chase: return art.walk[(int)(thing.walkTime * 6.f) % 4];
        case ThingState::Attack: return art.attack[thing.struck ? 1 : 0];
        case ThingState::Pain: return art.pain;
        case ThingState::Dying: return art.death[std::min(3, (int)(thing.stateTime / 0.16f))];
        case ThingState::Dead: return art.death[4];
        default: return art.walk[0];
    }
}

void Renderer::drawSprites(const Game& game, const Camera& camera, Image& frame)
{
    struct Billboard {
        const Sprite* sprite;
        float x, y;
        /// Height of the sprite's bottom edge above the floor.
        float base;
        float depth = 0.f;
        float across = 0.f;
    };

    std::vector<Billboard> billboards;

    for (const Thing& thing: game.things())
        if (!thing.removed)
            billboards.push_back({&thingSprite(thing), thing.x, thing.y, 0.f});

    for (const Projectile& projectile: game.projectiles())
    {
        const int frame = (int)(projectile.age * 10.f) % 2;
        const Sprite* sprite = projectile.type == ProjectileType::Rocket ? &m_Art.rocket
                               : projectile.type == ProjectileType::Plasma ? &m_Art.plasma[frame]
                                                                           : &m_Art.fireball[frame];
        const float centre = projectile.owner == PLAYER ? 0.36f : 0.32f;
        billboards.push_back({sprite, projectile.x, projectile.y, centre - sprite->height / 2.f});
    }

    for (const Effect& effect: game.effects())
    {
        const float t = effect.age / effectDuration(effect.type);
        auto pick = [t](const auto& frames) { return &frames[std::min((int)frames.size() - 1, (int)(t * (float)frames.size()))]; };
        const Sprite* sprite;
        switch (effect.type)
        {
            case EffectType::Blood: sprite = pick(m_Art.blood); break;
            case EffectType::Explosion: sprite = pick(m_Art.explosion); break;
            case EffectType::FireballBurst: sprite = pick(m_Art.fireballBurst); break;
            case EffectType::PlasmaBurst: sprite = pick(m_Art.plasmaBurst); break;
            default: sprite = pick(m_Art.puff); break;
        }
        // Explosions sit on the floor, the rest are centred where they hit.
        const float base = effect.type == EffectType::Explosion ? 0.f : effect.z - sprite->height / 2.f;
        billboards.push_back({sprite, effect.x, effect.y, base});
    }

    // Into camera space: how far in front of the camera, and how far across.
    const float determinant = camera.dirX * camera.planeY - camera.planeX * camera.dirY;
    for (Billboard& billboard: billboards)
    {
        const float rx = billboard.x - camera.x, ry = billboard.y - camera.y;
        billboard.depth = (rx * camera.planeY - camera.planeX * ry) / determinant;
        billboard.across = (camera.dirX * ry - rx * camera.dirY) / determinant;
    }
    std::erase_if(billboards, [](const Billboard& billboard) { return billboard.depth < 0.1f; });
    // Painter's order: farthest first.
    std::sort(billboards.begin(), billboards.end(),
              [](const Billboard& a, const Billboard& b) { return a.depth > b.depth; });

    const Map& map = game.map();
    for (const Billboard& billboard: billboards)
    {
        const Sprite& sprite = *billboard.sprite;
        const Image& image = sprite.image;
        if (image.empty())
            continue;

        const float height = sprite.height * camera.scale / billboard.depth;
        const float width = height * (float)image.width / (float)image.height;
        const float centre = (float)WIDTH / 2.f * (1.f + billboard.across / billboard.depth);
        const float left = centre - width / 2.f;
        const float bottom = camera.horizon + (camera.eye - billboard.base) * camera.scale / billboard.depth;
        const float top = bottom - height;

        const int x0 = std::max(0, (int)std::ceil(left - 0.5f));
        const int x1 = std::min(WIDTH, (int)std::ceil(left + width - 0.5f));
        const int y0 = std::max(0, (int)std::ceil(top - 0.5f));
        const int y1 = std::min(VIEW_HEIGHT, (int)std::ceil(bottom - 0.5f));

        const int shade = sprite.fullbright
                          ? 256
                          : toShade((lightAt(map, billboard.x, billboard.y) + m_Flash) * fog(billboard.depth));

        for (int x = x0; x < x1; ++x)
        {
            if (billboard.depth >= m_Depth[x])
                continue;
            const int column = std::clamp((int)(((float)x + 0.5f - left) / width * (float)image.width), 0, image.width - 1);

            for (int y = y0; y < y1; ++y)
            {
                const int texel = std::clamp((int)(((float)y + 0.5f - top) / height * (float)image.height), 0,
                                             image.height - 1);
                const std::uint32_t pixel = image.at(column, texel);
                if (alphaOf(pixel) >= 128)
                    frame.pixels[(std::size_t)y * WIDTH + x] = shadePixel(pixel, shade);
            }
        }
    }
}

void Renderer::drawWeapon(const Game& game, Image& frame)
{
    const Player& player = game.player();
    const WeaponArt& weapon = m_Art.weapons[(int)player.weapon];
    if (weapon.frames.empty())
        return;

    bool flash;
    const int index = weaponFrame(player.weapon, player.sinceShot, game.time(), flash);
    const Image& image = weapon.frames[std::min(index, (int)weapon.frames.size() - 1)];

    // Sway side to side and dip with each step, and drop out of sight while
    // switching weapons, or dying.
    const float swayX = std::sin(player.bobPhase) * 10.f * player.bob;
    const float swayY = std::fabs(std::cos(player.bobPhase)) * 7.f * player.bob;
    float lower = (1.f - player.weaponRaise) * (float)image.height;
    if (player.dead)
        lower += player.deadTime * 160.f;

    const int left = WIDTH / 2 - image.width / 2 + (int)swayX;
    const int top = VIEW_HEIGHT - image.height + 4 + (int)(swayY + lower);

    const Map& map = game.map();
    const float light = std::max(0.4f, map.light((int)player.x, (int)player.y)) + m_Flash;
    const int shade = toShade(light);

    auto draw = [&](const Image& picture, int x0, int y0, int pictureShade) {
        for (int y = std::max(0, -y0); y < picture.height && y0 + y < VIEW_HEIGHT; ++y)
            for (int x = std::max(0, -x0); x < picture.width && x0 + x < WIDTH; ++x)
            {
                const std::uint32_t pixel = picture.at(x, y);
                if (alphaOf(pixel) >= 128)
                    frame.pixels[(std::size_t)(y0 + y) * WIDTH + x0 + x] = shadePixel(pixel, pictureShade);
            }
    };

    if (flash && !weapon.flash.empty())
        draw(weapon.flash, left + weapon.flashX - weapon.flash.width / 2, top + weapon.flashY - weapon.flash.height / 2, 256);
    draw(image, left, top, shade);
}

void Renderer::drawTints(const Game& game, Image& frame)
{
    const Player& player = game.player();

    const float red = std::min(0.75f, player.damageFlash * 0.55f + (player.dead ? 0.35f : 0.f));
    const float gold = player.pickupFlash * 0.35f;
    if (red > 0.f || gold > 0.f)
        for (int i = 0; i < WIDTH * VIEW_HEIGHT; ++i)
        {
            std::uint32_t& pixel = frame.pixels[i];
            if (red > 0.f)
                pixel = blend(pixel, 255, 0, 0, red);
            if (gold > 0.f)
                pixel = blend(pixel, 255, 215, 110, gold);
        }

    // A small cross in the middle, to aim by.
    if (!player.dead && game.state() == GameState::Playing)
    {
        const int cx = WIDTH / 2, cy = VIEW_HEIGHT / 2;
        for (int d: {-4, -3, 3, 4})
        {
            frame.at(cx + d, cy) = blend(frame.at(cx + d, cy), 255, 255, 255, 0.6f);
            frame.at(cx, cy + d) = blend(frame.at(cx, cy + d), 255, 255, 255, 0.6f);
        }
    }
}

void Renderer::drawStatusBar(const Game& game, Image& frame)
{
    const Player& player = game.player();
    std::copy(m_StatusBar.pixels.begin(), m_StatusBar.pixels.end(), frame.pixels.begin() + (std::size_t)VIEW_HEIGHT * WIDTH);

    const std::uint32_t red = rgba(220, 30, 20);
    const std::uint32_t label = rgba(200, 190, 175);
    const int numbers = VIEW_HEIGHT + 4;
    const int labels = VIEW_HEIGHT + 24;

    auto readout = [&](int left, int right, const std::string& value, const std::string& name) {
        const int centre = (left + right) / 2;
        font::drawShadowedText(frame, centre - font::textWidth(value, 2) / 2, numbers, value, red, 2);
        font::drawShadowedText(frame, centre - font::textWidth(name) / 2, labels, name, label);
    };

    const AmmoType ammo = weaponInfo(player.weapon).ammo;
    readout(0, 76, ammo == AmmoType::None ? "" : std::to_string(player.ammo[(int)ammo]), "AMMO");
    readout(76, 166, std::to_string(player.health) + "%", "HEALTH");

    // The weapons carried, by the number that selects them.
    for (int slot = 1; slot < WEAPON_COUNT; ++slot)
    {
        const int x = 186 + (slot - 1) % 2 * 18, y = VIEW_HEIGHT + 4 + (slot - 1) / 2 * 10;
        const std::uint32_t colour = player.owned[slot] ? rgba(255, 215, 60) : rgba(110, 104, 96);
        font::drawShadowedText(frame, x, y, std::to_string(slot + 1), colour);
    }
    font::drawShadowedText(frame, 201 - font::textWidth("ARMS") / 2, labels, "ARMS", label);

    const LevelStats& stats = game.stats();
    readout(236, 296, std::to_string(stats.kills) + "/" + std::to_string(stats.totalKills), "KILLS");
    readout(296, 386, std::to_string(player.armor) + "%", "ARMOR");

    // Every kind of ammo, carried and most that can be carried.
    const char* names[] = {"BULL", "SHEL", "RCKT"};
    const int most[] = {200, 50, 50};
    for (int type = 0; type < AMMO_TYPE_COUNT; ++type)
    {
        const int y = VIEW_HEIGHT + 4 + type * 9;
        const bool current = (int)ammo == type;
        const std::uint32_t colour = current ? rgba(255, 215, 60) : rgba(200, 190, 175);
        font::drawShadowedText(frame, 392, y, names[type], colour);
        const std::string count = std::to_string(player.ammo[type]) + "/" + std::to_string(most[type]);
        font::drawShadowedText(frame, WIDTH - 4 - font::textWidth(count), y, count, colour);
    }
}

void Renderer::drawMessages(const Game& game, Image& frame)
{
    if (game.messageAge() < 3.f)
        font::drawShadowedText(frame, 4, 4, game.message(), rgba(230, 60, 50));

    const Player& player = game.player();
    if (player.dead && player.deadTime > 1.f && game.state() == GameState::Playing)
        centredText(frame, VIEW_HEIGHT / 2 - 10, "PRESS FIRE TO TRY AGAIN", rgba(240, 220, 200), 2);
}

void Renderer::drawAutomap(const Game& game, Image& frame)
{
    const Map& map = game.map();
    const Player& player = game.player();
    constexpr float CELL = 7.f;

    for (int i = 0; i < WIDTH * VIEW_HEIGHT; ++i)
        frame.pixels[i] = shadePixel(frame.pixels[i], 40);

    auto screenX = [&](float x) { return (float)WIDTH / 2.f + (x - player.x) * CELL; };
    auto screenY = [&](float y) { return (float)VIEW_HEIGHT / 2.f + (y - player.y) * CELL; };

    // Only what has been seen: each wall's faces that look onto open floor.
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x)
        {
            if (!m_Seen[(std::size_t)y * map.width() + x])
                continue;

            const Tile tile = map.at(x, y).tile;
            const float sx = screenX((float)x), sy = screenY((float)y);
            if (tile == Tile::Door)
            {
                const std::uint32_t colour = rgba(230, 200, 40);
                if (map.at(x, y).doorAlongX)
                    drawLine(frame, sx, sy + CELL / 2.f, sx + CELL, sy + CELL / 2.f, colour, VIEW_HEIGHT);
                else
                    drawLine(frame, sx + CELL / 2.f, sy, sx + CELL / 2.f, sy + CELL, colour, VIEW_HEIGHT);
                continue;
            }

            const std::uint32_t colour = tile == Tile::Exit ? rgba(60, 230, 60) : rgba(210, 50, 40);
            auto open = [&](int nx, int ny) { return map.inside(nx, ny) && !map.isWall(nx, ny); };
            if (open(x, y - 1)) drawLine(frame, sx, sy, sx + CELL, sy, colour, VIEW_HEIGHT);
            if (open(x, y + 1)) drawLine(frame, sx, sy + CELL, sx + CELL, sy + CELL, colour, VIEW_HEIGHT);
            if (open(x - 1, y)) drawLine(frame, sx, sy, sx, sy + CELL, colour, VIEW_HEIGHT);
            if (open(x + 1, y)) drawLine(frame, sx + CELL, sy, sx + CELL, sy + CELL, colour, VIEW_HEIGHT);
        }

    // The player: an arrow pointing the way they face.
    const float cx = (float)WIDTH / 2.f, cy = (float)VIEW_HEIGHT / 2.f;
    const float dx = std::cos(player.angle), dy = std::sin(player.angle);
    const std::uint32_t white = rgba(255, 255, 255);
    drawLine(frame, cx - dx * 5.f, cy - dy * 5.f, cx + dx * 6.f, cy + dy * 6.f, white, VIEW_HEIGHT);
    for (float side: {-1.f, 1.f})
        drawLine(frame, cx + dx * 6.f, cy + dy * 6.f, cx + dx * 2.f - dy * 3.f * side, cy + dy * 2.f + dx * 3.f * side,
                 white, VIEW_HEIGHT);

    font::drawShadowedText(frame, 4, VIEW_HEIGHT - 12, map.name(), rgba(230, 200, 40));
}

void Renderer::drawIntermission(const Game& game, Image& frame)
{
    for (std::uint32_t& pixel: frame.pixels)
        pixel = shadePixel(pixel, 60);

    const std::uint32_t red = rgba(225, 40, 30), white = rgba(240, 230, 215);
    const bool blink = std::fmod(game.time(), 1.f) < 0.6f;

    if (game.state() == GameState::Victory)
    {
        centredText(frame, 70, "VICTORY!", red, 4);
        centredText(frame, 130, "THE KEEP HAS FALLEN AND ITS", white, 1);
        centredText(frame, 142, "DEMONS LIE STILL. FOR NOW.", white, 1);
        if (blink)
            centredText(frame, 200, "PRESS FIRE TO PLAY AGAIN", white, 1);
        return;
    }

    const LevelStats& stats = game.stats();
    auto percent = [](int part, int whole) { return std::to_string(whole > 0 ? part * 100 / whole : 100) + "%"; };
    const int seconds = (int)stats.time;
    const std::string time = std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "")
                             + std::to_string(seconds % 60);

    centredText(frame, 40, game.map().name(), white, 2);
    centredText(frame, 62, "FINISHED", red, 3);

    const int left = 150, right = WIDTH - 150;
    int y = 110;
    for (auto [name, value]: {std::pair<std::string, std::string>{"KILLS", percent(stats.kills, stats.totalKills)},
                              {"ITEMS", percent(stats.items, stats.totalItems)},
                              {"TIME", time}})
    {
        font::drawShadowedText(frame, left, y, name, red, 2);
        font::drawShadowedText(frame, right - font::textWidth(value, 2), y, value, white, 2);
        y += 26;
    }

    if (blink)
        centredText(frame, 210, "PRESS FIRE TO CONTINUE", white, 1);
}

} // namespace doom
