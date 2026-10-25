#ifndef MAPLEDOOM_GAME_H
#define MAPLEDOOM_GAME_H

#include "Map.h"
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace doom {

/// The rules run at a fixed rate, whatever the frame rate.
constexpr float TICK = 1.f / 60.f;

/// Stands for the player wherever a thing index would go: who fired a shot,
/// who a monster is after.
constexpr int PLAYER = -1;
/// Damage from the level itself, such as nukage.
constexpr int WORLD = -2;

constexpr float PLAYER_RADIUS = 0.22f;

enum class Weapon : std::uint8_t { Fist, Pistol, Shotgun, Chaingun, RocketLauncher, Count };
enum class AmmoType : std::uint8_t { Bullets, Shells, Rockets, Count, None = Count };

constexpr int WEAPON_COUNT = (int)Weapon::Count;
constexpr int AMMO_TYPE_COUNT = (int)AmmoType::Count;

struct WeaponInfo {
    const char* name;
    AmmoType ammo;
    /// Seconds from one shot to the next.
    float cooldown;
};

const WeaponInfo& weaponInfo(Weapon weapon);

/// What the player wants to do this tick.
struct Input {
    /// -1 to 1: backwards to forwards.
    float forward = 0.f;
    /// -1 to 1: left to right.
    float strafe = 0.f;
    /// Radians to turn by this tick, clockwise.
    float turn = 0.f;
    bool run = false;
    /// Held down.
    bool fire = false;
    /// Pressed this tick: open doors, flip switches.
    bool use = false;
    /// A weapon chosen this tick, or Count for none.
    Weapon select = Weapon::Count;
    /// 1 for the next weapon, -1 for the previous, 0 for neither.
    int cycle = 0;
};

struct Player {
    float x = 0.f, y = 0.f;
    float angle = 0.f;
    float vx = 0.f, vy = 0.f;

    int health = 100;
    int armor = 0;
    std::array<int, AMMO_TYPE_COUNT> ammo{};
    std::array<bool, WEAPON_COUNT> owned{};
    Weapon weapon = Weapon::Pistol;
    /// The weapon being switched to: the current one once it is up.
    Weapon pendingWeapon = Weapon::Pistol;
    /// 1 with the weapon raised and ready, 0 lowered out of sight.
    float weaponRaise = 1.f;
    /// Seconds until the weapon may fire again.
    float cooldown = 0.f;
    /// Seconds since the last shot, for the firing animation.
    float sinceShot = 10.f;

    /// Walking, for the view and the weapon to bob with: how fast, from 0
    /// standing to 1 running, and how far through a stride.
    float bob = 0.f;
    float bobPhase = 0.f;

    /// Full screen tints, 0 to 1, that fade away: red when hurt, gold on
    /// picking something up.
    float damageFlash = 0.f;
    float pickupFlash = 0.f;

    bool dead = false;
    float deadTime = 0.f;
};

enum class ThingState : std::uint8_t { Idle, Chase, Attack, Pain, Dying, Dead };

/// Anything in the level other than the player and the walls: monsters,
/// scenery and pickups.
struct Thing {
    ThingType type;
    float x, y;
    ThingState state = ThingState::Idle;
    /// Seconds since the state began.
    float stateTime = 0.f;
    int health = 0;
    /// Facing, in radians like the player's.
    float angle = 0.f;
    /// Monsters: who they are after, PLAYER or another thing's index.
    /// Barrels: who set them off.
    int target = PLAYER;
    float attackCooldown = 0.f;
    /// Whether the current attack has struck yet.
    bool struck = false;
    /// Seconds spent walking, for the walk animation.
    float walkTime = 0.f;
    /// Seconds the monster has been unable to move where it wants.
    float stuckTime = 0.f;
    /// Monsters steer around each other by turning one way or the other.
    float dodge = 0.f;
    /// Picked up, or blown to bits.
    bool removed = false;
    /// Dropped by a monster, and so not counted among the level's items.
    bool dropped = false;
};

/// How big a thing is and how tough.
struct ThingInfo {
    float radius;
    int health;
};

const ThingInfo& thingInfo(ThingType type);

enum class ProjectileType : std::uint8_t { Fireball, Plasma, Rocket };

struct Projectile {
    ProjectileType type;
    float x, y;
    float vx, vy;
    int owner;
    float age = 0.f;
    bool alive = true;
};

enum class EffectType : std::uint8_t {
    /// A bullet striking a wall.
    Puff,
    /// A bullet striking flesh.
    Blood,
    /// A rocket or barrel going off.
    Explosion,
    /// Fireballs and plasma bursting.
    FireballBurst,
    PlasmaBurst
};

/// A short animation in the world that affects nothing.
struct Effect {
    EffectType type;
    float x, y;
    /// Height above the floor, in wall heights.
    float z;
    float age = 0.f;
};

/// How long each effect lasts, in seconds.
float effectDuration(EffectType type);

enum class Sound : std::uint8_t {
    Punch, Pistol, Shotgun, Chaingun, RocketLaunch, NoAmmo,
    Explosion, FireballLaunch, FireballBurst, PlasmaLaunch, PlasmaBurst,
    TrooperShot, Claw, Bite,
    /// Monster sounds, told apart by the source.
    MonsterAlert, MonsterPain, MonsterDeath,
    PlayerPain, PlayerDeath, PlayerGrunt,
    DoorOpen, DoorClose,
    ItemPickup, WeaponPickup, ExitSwitch,
    Count
};

struct SoundEvent {
    Sound sound;
    /// Who made it, for monster sounds.
    ThingType source;
    float x, y;
};

enum class GameState : std::uint8_t {
    Playing,
    /// The exit switch was flipped: the tally screen shows until the player
    /// fires or uses to go on.
    LevelComplete,
    /// The last level is done.
    Victory
};

struct LevelStats {
    int kills = 0;
    int totalKills = 0;
    int items = 0;
    int totalItems = 0;
    float time = 0.f;
};

/// The whole game, with no engine dependencies: give it each tick's input
/// and it moves the player, runs the monsters and the doors, and keeps
/// score. The controller plays its sounds and the renderer draws it.
class Game {
public:
    explicit Game(std::vector<MapDefinition> levels, std::uint32_t seed = 1);

    void tick(const Input& input);

    /// Start a level over, keeping what the player carries or not.
    void loadLevel(int index, bool keepInventory);

    GameState state() const { return m_State; }
    int levelIndex() const { return m_Level; }
    int levelCount() const { return (int)m_Levels.size(); }
    /// Goes up every time a level starts, so views can tell.
    int levelSerial() const { return m_LevelSerial; }
    float time() const { return m_Time; }

    const Map& map() const { return m_Map; }
    const Player& player() const { return m_Player; }
    const std::vector<Thing>& things() const { return m_Things; }
    const std::vector<Projectile>& projectiles() const { return m_Projectiles; }
    const std::vector<Effect>& effects() const { return m_Effects; }
    const LevelStats& stats() const { return m_Stats; }

    /// The latest pickup or event message, and how long it has shown.
    const std::string& message() const { return m_Message; }
    float messageAge() const { return m_MessageAge; }

    /// The sounds made since the last call.
    std::vector<SoundEvent> takeSounds();

    /// For tests: set the player down somewhere.
    void placePlayer(float x, float y, float angle);
    /// For tests: hurt a thing or the player.
    void damage(int target, int amount, int source);

private:
    void updatePlayer(const Input& input);
    void movePlayer(const Input& input);
    void updateWeapon(const Input& input);
    void fireWeapon();
    void switchToBestWeapon();
    void use();
    void collectPickups();
    bool pickUp(Thing& thing);

    void updateDoors();
    void openDoor(int x, int y);
    bool doorBlocked(int x, int y) const;

    void updateThings();
    void updateMonster(int index);
    void lookForPlayer(Thing& monster);
    void chase(int index);
    bool pathDirection(const Thing& monster, float& dx, float& dy) const;
    void attack(int index);
    void strike(int index);
    void moveMonster(int index, float dx, float dy, float distance);
    void wake(Thing& monster, bool alertSound);
    void killMonster(Thing& monster);
    void explodeBarrel(Thing& barrel);

    void updateProjectiles();
    void spawnProjectile(ProjectileType type, int owner, float x, float y, float angle);
    void burst(Projectile& projectile, int victim);

    void updateEffects();
    void addEffect(EffectType type, float x, float y, float z);

    /// Fire a bullet, or throw a punch with a short range. Returns what it
    /// hit: a thing's index, PLAYER, or WORLD for a wall or nothing.
    int hitscan(int shooter, float x, float y, float angle, float range, int damage);
    /// Hurt everything within `radius` of a blast that can see it.
    void radiusDamage(float x, float y, int maximum, float radius, int source);
    void damagePlayer(int amount, int source);

    /// Wake the monsters that can hear a shot from (x, y): sound carries
    /// through open space and open doors, not through walls.
    void makeNoise(float x, float y);
    /// Refresh the map of steps to the player, which monsters that cannot
    /// see the player follow.
    void updatePathing();

    /// Whether a body of `radius` at (x, y) would overlap a wall or a shut
    /// door.
    bool wallBlocks(float x, float y, float radius) const;
    /// Whether a body of `radius` may move from (fromX, fromY) to (x, y).
    /// Bodies already overlapping may always move apart.
    bool canMove(int self, float fromX, float fromY, float x, float y, float radius) const;
    bool isSolid(const Thing& thing) const;
    bool isShootable(const Thing& thing) const;
    bool isAlive(int target) const;
    float targetX(int target) const;
    float targetY(int target) const;
    float targetRadius(int target) const;

    void playSound(Sound sound, float x, float y, ThingType source = ThingType::Count);
    void showMessage(const std::string& message);
    int roll(int low, int high);
    float randomFloat(float low, float high);
    bool chance(float probability);

    std::vector<MapDefinition> m_Levels;
    int m_Level = 0;
    int m_LevelSerial = 0;
    Map m_Map;
    Player m_Player;
    std::vector<Thing> m_Things;
    /// Things made during a tick, added at its end so indices stay put.
    std::vector<Thing> m_NewThings;
    std::vector<Projectile> m_Projectiles;
    std::vector<Effect> m_Effects;
    std::vector<SoundEvent> m_Sounds;

    struct Door {
        int x, y;
        enum class State : std::uint8_t { Closed, Opening, Open, Closing } state = State::Closed;
        float timer = 0.f;
    };
    std::vector<Door> m_Doors;

    /// Steps from each cell to the player's, or -1 where the player cannot
    /// be reached.
    std::vector<int> m_Paths;
    int m_PathCellX = -1;
    int m_PathCellY = -1;
    float m_PathAge = 0.f;

    GameState m_State = GameState::Playing;
    LevelStats m_Stats;
    std::string m_Message;
    float m_MessageAge = 100.f;
    float m_Time = 0.f;
    float m_NukageTimer = 0.f;
    float m_PainSoundTimer = 0.f;
    bool m_FireWasHeld = false;

    std::mt19937 m_Random;
};

} // namespace doom

#endif //MAPLEDOOM_GAME_H
