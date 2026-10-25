#ifndef MAPLEDOOM_ART_H
#define MAPLEDOOM_ART_H

#include "Game.h"
#include "Rendering/Image.h"
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace doom {

/// A picture standing in the world, always turned to face the camera.
struct Sprite {
    /// Pixels with alpha below one half are see through.
    Image image;
    /// How tall it stands, in wall heights. The width follows from the
    /// image's shape.
    float height = 0.5f;
    /// Drawn at full brightness however dark it is around it: fire, lamps.
    bool fullbright = false;
};

struct MonsterArt {
    std::array<Sprite, 4> walk;
    /// Winding up, then striking.
    std::array<Sprite, 2> attack;
    Sprite pain;
    /// Falling down, ending with the corpse.
    std::array<Sprite, 5> death;
};

/// A weapon as seen in the player's hands, drawn at the frame's pixel scale
/// and standing on the bottom of the view.
struct WeaponArt {
    /// 0 at rest, then the firing frames.
    std::vector<Image> frames;
    /// The muzzle flash, drawn centred on (flashX, flashY) of a frame. Empty
    /// for the fist.
    Image flash;
    int flashX = 0;
    int flashY = 0;
};

/// Every texture and sprite in the game.
///
/// All of it is drawn in code when the game starts, so the game needs no
/// files. Any of it can still be replaced by an image file of the same name:
/// see loadArtOverrides() and saveArt().
struct Art {
    std::array<Image, (int)WallTexture::Count> walls;

    Image floorStone;
    Image floorMetal;
    Image floorDirt;
    Image nukage;
    Image ceilingStone;
    Image ceilingMetal;
    /// Wraps round the whole horizon twice.
    Image sky;

    /// By ThingType, monsters first.
    std::array<MonsterArt, 4> monsters;
    /// By ThingType, for everything but monsters. The monster slots are
    /// left empty.
    std::array<Sprite, (int)ThingType::Count> things;

    std::array<Sprite, 3> puff;
    std::array<Sprite, 3> blood;
    std::array<Sprite, 4> explosion;
    std::array<Sprite, 2> fireball;
    std::array<Sprite, 3> fireballBurst;
    std::array<Sprite, 2> plasma;
    std::array<Sprite, 3> plasmaBurst;
    Sprite rocket;

    std::array<WeaponArt, WEAPON_COUNT> weapons;
};

Art createArt();

/// Every picture in `art`, with the file name (without .png) it is saved
/// and looked for under.
std::vector<std::pair<std::string, Image*>> artFiles(Art& art);

/// Replace pictures with any PNG files in `directory` named as artFiles()
/// names them. Returns how many were replaced.
int loadArtOverrides(Art& art, const std::string& directory);

/// Save every picture in `art` to `directory` as PNG, to start new art
/// from. Returns how many were saved.
int saveArt(Art& art, const std::string& directory);

} // namespace doom

#endif //MAPLEDOOM_ART_H
