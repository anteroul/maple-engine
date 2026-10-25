#include "Game.h"
#include "Raycast.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace doom {

namespace {

constexpr float PI = std::numbers::pi_v<float>;

// Player movement, in cells per second.
constexpr float WALK_SPEED = 3.6f;
constexpr float RUN_SPEED = 6.2f;
/// How quickly the player gets up to speed and stops again, per second.
constexpr float ACCELERATION = 14.f;
constexpr float USE_RANGE = 1.3f;
constexpr float MELEE_RANGE = 1.1f;
constexpr float HITSCAN_RANGE = 64.f;
/// Weapon switches per second: the old one goes down, the new one comes up.
constexpr float WEAPON_SWAP_SPEED = 6.f;
constexpr std::array<int, AMMO_TYPE_COUNT> MAX_AMMO = {200, 50, 50};

constexpr float DOOR_SPEED = 2.f;
/// Seconds a door stays open before it starts closing.
constexpr float DOOR_WAIT = 4.f;

constexpr float SIGHT_RANGE = 30.f;
/// Monsters see this far either side of the way they face, in radians.
constexpr float SIGHT_HALF_ANGLE = 1.75f;
/// Cells a shot can be heard through.
constexpr int HEARING_STEPS = 40;

constexpr float BARREL_FUSE = 0.2f;
constexpr int BLAST_DAMAGE = 100;
constexpr float BLAST_RADIUS = 1.6f;
constexpr float PROJECTILE_RADIUS = 0.12f;

/// How each kind of monster fights.
struct MonsterInfo {
    /// Cells per second.
    float speed;
    float painChance;
    /// Seconds a monster flinches for when it is hurt.
    float painTime;
    /// How close the monster has to be to strike in melee: the gap between
    /// its edge and its target's. 0 for monsters with no melee attack.
    float meleeRange;
    bool ranged;
    /// Seconds into an attack that it strikes, and the attack's length.
    float windup;
    float attackTime;
    /// Seconds before attacking again.
    float cooldownMin;
    float cooldownMax;
    float deathTime;
};

const MonsterInfo& monsterInfo(ThingType type)
{
    static const MonsterInfo info[] = {
            /* Trooper */ {1.2f, 0.78f, 0.25f, 0.f, true, 0.3f, 0.55f, 1.f, 2.6f, 0.6f},
            /* Imp */ {1.4f, 0.78f, 0.25f, 0.35f, true, 0.45f, 0.8f, 1.1f, 2.8f, 0.7f},
            /* Demon */ {2.7f, 0.7f, 0.3f, 0.35f, false, 0.3f, 0.6f, 0.3f, 0.7f, 0.7f},
            /* Knight */ {1.4f, 0.25f, 0.3f, 0.4f, true, 0.5f, 0.9f, 1.f, 2.2f, 1.f},
    };
    return info[(int)type];
}

struct ProjectileInfo {
    float speed;
    /// Damage is this times a roll of 1 to 8.
    int damage;
};

const ProjectileInfo& projectileInfo(ProjectileType type)
{
    static const ProjectileInfo info[] = {
            /* Fireball */ {4.5f, 3},
            /* Plasma */ {6.f, 8},
            /* Rocket */ {12.f, 20},
    };
    return info[(int)type];
}

float wrapAngle(float angle)
{
    angle = std::fmod(angle + PI, 2.f * PI);
    if (angle < 0.f)
        angle += 2.f * PI;
    return angle - PI;
}

/// How far a body of `radius` at (x, y) reaches into the cell, if at all.
bool overlapsCell(float x, float y, float radius, int cellX, int cellY)
{
    const float nearestX = std::clamp(x, (float)cellX, (float)cellX + 1.f);
    const float nearestY = std::clamp(y, (float)cellY, (float)cellY + 1.f);
    return std::hypot(x - nearestX, y - nearestY) < radius;
}

} // namespace

const WeaponInfo& weaponInfo(Weapon weapon)
{
    static const WeaponInfo info[] = {
            {"FIST", AmmoType::None, 0.5f},
            {"PISTOL", AmmoType::Bullets, 0.38f},
            {"SHOTGUN", AmmoType::Shells, 0.95f},
            {"CHAINGUN", AmmoType::Bullets, 0.11f},
            {"ROCKET LAUNCHER", AmmoType::Rockets, 0.6f},
    };
    return info[(int)weapon];
}

const ThingInfo& thingInfo(ThingType type)
{
    static const ThingInfo info[] = {
            /* Trooper */ {0.22f, 20},
            /* Imp */ {0.22f, 60},
            /* Demon */ {0.32f, 150},
            /* Knight */ {0.28f, 500},
            /* Barrel */ {0.18f, 20},
            /* Lamp */ {0.16f, 0},
            /* Column */ {0.2f, 0},
            /* Medkit */ {0.2f, 0},
            /* Stimpack */ {0.2f, 0},
            /* Armor */ {0.2f, 0},
            /* Clip */ {0.2f, 0},
            /* Shells */ {0.2f, 0},
            /* RocketBox */ {0.2f, 0},
            /* Shotgun */ {0.2f, 0},
            /* Chaingun */ {0.2f, 0},
            /* RocketLauncher */ {0.2f, 0},
    };
    return info[(int)type];
}

float effectDuration(EffectType type)
{
    switch (type)
    {
        case EffectType::Explosion: return 0.6f;
        default: return 0.35f;
    }
}

Game::Game(std::vector<MapDefinition> levels, std::uint32_t seed)
    : m_Levels(std::move(levels)), m_Map(m_Levels.at(0)), m_Random(seed)
{
    loadLevel(0, false);
}

void Game::loadLevel(int index, bool keepInventory)
{
    m_Level = index;
    ++m_LevelSerial;
    m_Map = Map(m_Levels.at(index));
    m_Things.clear();
    m_NewThings.clear();
    m_Projectiles.clear();
    m_Effects.clear();
    m_Doors.clear();
    m_Stats = {};

    for (const ThingSpawn& spawn: m_Map.spawns())
    {
        Thing thing{spawn.type, spawn.x, spawn.y};
        thing.health = thingInfo(spawn.type).health;
        // Monsters stand facing any which way, so some can be crept up on.
        thing.angle = (float)roll(0, 3) * PI / 2.f;
        thing.dodge = chance(0.5f) ? 1.f : -1.f;
        if (isMonster(spawn.type))
            ++m_Stats.totalKills;
        if (isPickup(spawn.type))
            ++m_Stats.totalItems;
        m_Things.push_back(thing);
    }

    for (int y = 0; y < m_Map.height(); ++y)
        for (int x = 0; x < m_Map.width(); ++x)
            if (m_Map.at(x, y).tile == Tile::Door)
                m_Doors.push_back({x, y});

    Player& player = m_Player;
    if (!keepInventory)
    {
        player = Player{};
        player.ammo[(int)AmmoType::Bullets] = 50;
        player.owned[(int)Weapon::Fist] = true;
        player.owned[(int)Weapon::Pistol] = true;
    }
    player.x = m_Map.startX();
    player.y = m_Map.startY();
    player.angle = m_Map.startAngle();
    player.vx = player.vy = 0.f;
    player.dead = false;
    player.deadTime = 0.f;
    player.pendingWeapon = player.weapon;
    player.weaponRaise = 1.f;
    player.cooldown = 0.f;
    player.sinceShot = 10.f;
    player.bob = 0.f;
    player.damageFlash = 0.f;
    player.pickupFlash = 0.f;

    m_State = GameState::Playing;
    m_NukageTimer = 0.f;
    m_PathCellX = m_PathCellY = -1;
    updatePathing();
    showMessage(m_Map.name());
}

void Game::tick(const Input& input)
{
    m_Time += TICK;
    m_MessageAge += TICK;
    const bool firePressed = input.fire && !m_FireWasHeld;
    m_FireWasHeld = input.fire;
    const bool proceed = firePressed || input.use;

    if (m_State != GameState::Playing)
    {
        if (proceed)
        {
            if (m_State == GameState::Victory)
                loadLevel(0, false);
            else if (m_Level + 1 < levelCount())
                loadLevel(m_Level + 1, true);
            else
                m_State = GameState::Victory;
        }
        return;
    }

    // Dying sends the player back to the start of the level with only a
    // pistol, as in Doom.
    if (m_Player.dead && m_Player.deadTime > 1.f && proceed)
    {
        loadLevel(m_Level, false);
        return;
    }

    m_Stats.time += TICK;
    updatePlayer(input);
    updateDoors();
    updatePathing();
    updateThings();
    updateProjectiles();
    updateEffects();

    m_Things.insert(m_Things.end(), m_NewThings.begin(), m_NewThings.end());
    m_NewThings.clear();
}

std::vector<SoundEvent> Game::takeSounds()
{
    std::vector<SoundEvent> sounds;
    sounds.swap(m_Sounds);
    return sounds;
}

void Game::placePlayer(float x, float y, float angle)
{
    m_Player.x = x;
    m_Player.y = y;
    m_Player.angle = angle;
    m_Player.vx = m_Player.vy = 0.f;
}

// ---------------------------------------------------------------- player --

void Game::updatePlayer(const Input& input)
{
    Player& player = m_Player;
    player.damageFlash = std::max(0.f, player.damageFlash - TICK * 1.2f);
    player.pickupFlash = std::max(0.f, player.pickupFlash - TICK * 2.5f);
    m_PainSoundTimer -= TICK;

    if (player.dead)
    {
        player.deadTime += TICK;
        player.bob = 0.f;
        return;
    }

    player.angle = wrapAngle(player.angle + input.turn);
    movePlayer(input);
    updateWeapon(input);
    if (input.use)
        use();
    collectPickups();

    // Nukage burns a little every second, starting shortly after stepping in.
    const Cell& cell = m_Map.at((int)player.x, (int)player.y);
    if (cell.floor == Floor::Nukage && cell.tile == Tile::Empty)
    {
        m_NukageTimer += TICK;
        if (m_NukageTimer >= 1.f)
        {
            m_NukageTimer -= 1.f;
            damagePlayer(roll(5, 10), WORLD);
        }
    }
    else
    {
        m_NukageTimer = 0.5f;
    }
}

void Game::movePlayer(const Input& input)
{
    Player& player = m_Player;
    const float cosine = std::cos(player.angle);
    const float sine = std::sin(player.angle);

    // Forward is along the facing, right a quarter turn clockwise from it.
    float wishX = cosine * input.forward - sine * input.strafe;
    float wishY = sine * input.forward + cosine * input.strafe;
    const float length = std::hypot(wishX, wishY);
    if (length > 1.f)
    {
        wishX /= length;
        wishY /= length;
    }

    const float speed = input.run ? RUN_SPEED : WALK_SPEED;
    const float blend = std::min(1.f, ACCELERATION * TICK);
    player.vx += (wishX * speed - player.vx) * blend;
    player.vy += (wishY * speed - player.vy) * blend;

    // One axis at a time, so the player slides along walls rather than
    // sticking to them.
    const float x = player.x + player.vx * TICK;
    if (!wallBlocks(x, player.y, PLAYER_RADIUS))
        player.x = x;
    else
        player.vx = 0.f;

    const float y = player.y + player.vy * TICK;
    if (!wallBlocks(player.x, y, PLAYER_RADIUS))
        player.y = y;
    else
        player.vy = 0.f;

    // Then out of anything round the player walked into, along the line
    // between them, so the player slides round columns and monsters too.
    for (const Thing& thing: m_Things)
    {
        if (!isSolid(thing))
            continue;
        const float dx = player.x - thing.x, dy = player.y - thing.y;
        const float distance = std::hypot(dx, dy);
        const float reach = PLAYER_RADIUS + thingInfo(thing.type).radius;
        if (distance >= reach || distance < 1e-4f)
            continue;

        const float pushedX = thing.x + dx / distance * reach, pushedY = thing.y + dy / distance * reach;
        if (!wallBlocks(pushedX, pushedY, PLAYER_RADIUS))
        {
            player.x = pushedX;
            player.y = pushedY;
        }
    }

    const float moving = std::hypot(player.vx, player.vy);
    player.bob = std::min(1.f, moving / RUN_SPEED);
    player.bobPhase += moving * TICK * 2.4f;
}

void Game::updateWeapon(const Input& input)
{
    Player& player = m_Player;

    if (input.select != Weapon::Count && player.owned[(int)input.select])
        player.pendingWeapon = input.select;

    if (input.cycle != 0)
    {
        int weapon = (int)player.pendingWeapon;
        for (int i = 0; i < WEAPON_COUNT; ++i)
        {
            weapon = (weapon + input.cycle + WEAPON_COUNT) % WEAPON_COUNT;
            if (player.owned[weapon])
            {
                player.pendingWeapon = (Weapon)weapon;
                break;
            }
        }
    }

    // Lower the old weapon all the way before raising the new one.
    if (player.pendingWeapon != player.weapon)
    {
        player.weaponRaise -= TICK * WEAPON_SWAP_SPEED;
        if (player.weaponRaise <= 0.f)
        {
            player.weaponRaise = 0.f;
            player.weapon = player.pendingWeapon;
        }
    }
    else
    {
        player.weaponRaise = std::min(1.f, player.weaponRaise + TICK * WEAPON_SWAP_SPEED);
    }

    player.cooldown = std::max(0.f, player.cooldown - TICK);
    player.sinceShot += TICK;

    if (!input.fire || player.pendingWeapon != player.weapon || player.weaponRaise < 1.f
        || player.cooldown > 0.f)
        return;

    const WeaponInfo& info = weaponInfo(player.weapon);
    if (info.ammo != AmmoType::None)
    {
        int& ammo = player.ammo[(int)info.ammo];
        if (ammo <= 0)
        {
            switchToBestWeapon();
            return;
        }
        --ammo;
    }

    player.cooldown = info.cooldown;
    fireWeapon();
    player.sinceShot = 0.f;
}

void Game::fireWeapon()
{
    const Player& player = m_Player;
    // The first shot after a pause goes exactly where it is aimed, the rest
    // of a burst wander a little.
    const bool steady = player.sinceShot > 0.6f;

    switch (player.weapon)
    {
        case Weapon::Fist:
            if (hitscan(PLAYER, player.x, player.y, player.angle, MELEE_RANGE, 2 * roll(1, 10)) >= 0)
                playSound(Sound::Punch, player.x, player.y);
            // Punching is quiet.
            return;
        case Weapon::Pistol:
            hitscan(PLAYER, player.x, player.y, player.angle + (steady ? 0.f : randomFloat(-0.045f, 0.045f)),
                    HITSCAN_RANGE, 5 * roll(1, 3));
            playSound(Sound::Pistol, player.x, player.y);
            break;
        case Weapon::Shotgun:
            for (int pellet = 0; pellet < 7; ++pellet)
                hitscan(PLAYER, player.x, player.y, player.angle + randomFloat(-0.09f, 0.09f),
                        HITSCAN_RANGE, 5 * roll(1, 3));
            playSound(Sound::Shotgun, player.x, player.y);
            break;
        case Weapon::Chaingun:
            hitscan(PLAYER, player.x, player.y, player.angle + (steady ? 0.f : randomFloat(-0.05f, 0.05f)),
                    HITSCAN_RANGE, 5 * roll(1, 3));
            playSound(Sound::Chaingun, player.x, player.y);
            break;
        case Weapon::RocketLauncher:
            spawnProjectile(ProjectileType::Rocket, PLAYER, player.x, player.y, player.angle);
            playSound(Sound::RocketLaunch, player.x, player.y);
            break;
        default:
            break;
    }

    makeNoise(player.x, player.y);
}

/// Out of ammo: change to the best weapon that still has some. The rocket
/// launcher is never picked this way, since it is too easy to blow oneself
/// up with.
void Game::switchToBestWeapon()
{
    Player& player = m_Player;
    auto usable = [&](Weapon weapon) {
        const AmmoType ammo = weaponInfo(weapon).ammo;
        return player.owned[(int)weapon] && (ammo == AmmoType::None || player.ammo[(int)ammo] > 0);
    };

    for (Weapon weapon: {Weapon::Chaingun, Weapon::Shotgun, Weapon::Pistol, Weapon::Fist})
        if (usable(weapon))
        {
            if (weapon != player.weapon)
            {
                player.pendingWeapon = weapon;
                playSound(Sound::NoAmmo, player.x, player.y);
            }
            return;
        }
}

void Game::use()
{
    const Player& player = m_Player;
    const RayHit hit = castRay(m_Map, player.x, player.y, std::cos(player.angle), std::sin(player.angle), USE_RANGE);
    if (!hit.hit)
        return;

    if (hit.tile == Tile::Door)
    {
        openDoor(hit.cellX, hit.cellY);
    }
    else if (hit.tile == Tile::Exit)
    {
        m_State = GameState::LevelComplete;
        playSound(Sound::ExitSwitch, player.x, player.y);
    }
    else
    {
        // Nothing to use here.
        playSound(Sound::PlayerGrunt, player.x, player.y);
    }
}

void Game::collectPickups()
{
    Player& player = m_Player;
    for (Thing& thing: m_Things)
    {
        if (thing.removed || !isPickup(thing.type))
            continue;
        if (std::hypot(thing.x - player.x, thing.y - player.y) > PLAYER_RADIUS + 0.3f)
            continue;
        if (!pickUp(thing))
            continue;

        thing.removed = true;
        if (!thing.dropped)
            ++m_Stats.items;
        player.pickupFlash = 0.4f;
    }
}

/// Give the player what the pickup holds. Returns false, leaving the pickup
/// where it is, if the player has no use for it yet.
bool Game::pickUp(Thing& thing)
{
    Player& player = m_Player;

    auto give = [&](AmmoType type, int amount) {
        int& ammo = player.ammo[(int)type];
        if (ammo >= MAX_AMMO[(int)type])
            return false;
        ammo = std::min(MAX_AMMO[(int)type], ammo + amount);
        return true;
    };
    auto giveWeapon = [&](Weapon weapon, AmmoType type, int amount, const char* message) {
        const bool fresh = !player.owned[(int)weapon];
        player.owned[(int)weapon] = true;
        const bool tookAmmo = give(type, amount);
        if (!fresh && !tookAmmo)
            return false;
        // A new weapon is switched to straight away.
        if (fresh)
            player.pendingWeapon = weapon;
        showMessage(message);
        playSound(Sound::WeaponPickup, player.x, player.y);
        return true;
    };
    auto took = [&](const char* message) {
        showMessage(message);
        playSound(Sound::ItemPickup, player.x, player.y);
        return true;
    };

    switch (thing.type)
    {
        case ThingType::Medkit:
            if (player.health >= 100)
                return false;
            player.health = std::min(100, player.health + 25);
            return took("PICKED UP A MEDIKIT.");
        case ThingType::Stimpack:
            if (player.health >= 100)
                return false;
            player.health = std::min(100, player.health + 10);
            return took("PICKED UP A STIMPACK.");
        case ThingType::Armor:
            if (player.armor >= 100)
                return false;
            player.armor = 100;
            return took("PICKED UP THE ARMOR.");
        case ThingType::Clip:
            return give(AmmoType::Bullets, thing.dropped ? 5 : 10) && took("PICKED UP A CLIP.");
        case ThingType::Shells:
            return give(AmmoType::Shells, 8) && took("PICKED UP SOME SHOTGUN SHELLS.");
        case ThingType::RocketBox:
            return give(AmmoType::Rockets, 3) && took("PICKED UP SOME ROCKETS.");
        case ThingType::Shotgun:
            return giveWeapon(Weapon::Shotgun, AmmoType::Shells, 8, "YOU GOT THE SHOTGUN!");
        case ThingType::Chaingun:
            return giveWeapon(Weapon::Chaingun, AmmoType::Bullets, 20, "YOU GOT THE CHAINGUN!");
        case ThingType::RocketLauncher:
            return giveWeapon(Weapon::RocketLauncher, AmmoType::Rockets, 4, "YOU GOT THE ROCKET LAUNCHER!");
        default:
            return false;
    }
}

void Game::damagePlayer(int amount, int)
{
    Player& player = m_Player;
    if (player.dead || m_State != GameState::Playing || amount <= 0)
        return;

    // Armor soaks up a third of the damage, for as long as it lasts.
    const int saved = std::min(player.armor, amount / 3);
    player.armor -= saved;
    player.health -= amount - saved;
    player.damageFlash = std::min(1.f, player.damageFlash + 0.1f + (float)amount / 30.f);

    if (player.health <= 0)
    {
        player.health = 0;
        player.dead = true;
        player.deadTime = 0.f;
        playSound(Sound::PlayerDeath, player.x, player.y);
        showMessage("YOU DIED. PRESS FIRE TO TRY AGAIN.");
    }
    else if (m_PainSoundTimer <= 0.f)
    {
        playSound(Sound::PlayerPain, player.x, player.y);
        m_PainSoundTimer = 0.35f;
    }
}

// ----------------------------------------------------------------- doors --

void Game::updateDoors()
{
    for (Door& door: m_Doors)
    {
        Cell& cell = m_Map.at(door.x, door.y);
        const float x = (float)door.x + 0.5f, y = (float)door.y + 0.5f;

        switch (door.state)
        {
            case Door::State::Opening:
                cell.open = std::min(1.f, cell.open + TICK * DOOR_SPEED);
                if (cell.open >= 1.f)
                {
                    door.state = Door::State::Open;
                    door.timer = DOOR_WAIT;
                }
                break;
            case Door::State::Open:
                door.timer -= TICK;
                if (door.timer > 0.f)
                    break;
                if (doorBlocked(door.x, door.y))
                {
                    door.timer = 0.5f;
                }
                else
                {
                    door.state = Door::State::Closing;
                    playSound(Sound::DoorClose, x, y);
                }
                break;
            case Door::State::Closing:
                // Never shut on anyone: open up again instead.
                if (doorBlocked(door.x, door.y))
                {
                    door.state = Door::State::Opening;
                    playSound(Sound::DoorOpen, x, y);
                    break;
                }
                cell.open = std::max(0.f, cell.open - TICK * DOOR_SPEED);
                if (cell.open <= 0.f)
                    door.state = Door::State::Closed;
                break;
            case Door::State::Closed:
                break;
        }
    }
}

void Game::openDoor(int x, int y)
{
    for (Door& door: m_Doors)
    {
        if (door.x != x || door.y != y)
            continue;

        if (door.state == Door::State::Closed || door.state == Door::State::Closing)
        {
            door.state = Door::State::Opening;
            playSound(Sound::DoorOpen, (float)x + 0.5f, (float)y + 0.5f);
        }
        else if (door.state == Door::State::Open)
        {
            door.timer = DOOR_WAIT;
        }
        return;
    }
}

bool Game::doorBlocked(int x, int y) const
{
    if (overlapsCell(m_Player.x, m_Player.y, PLAYER_RADIUS, x, y))
        return true;

    for (const Thing& thing: m_Things)
        if (isSolid(thing) && overlapsCell(thing.x, thing.y, thingInfo(thing.type).radius, x, y))
            return true;
    return false;
}

// -------------------------------------------------------------- monsters --

void Game::updateThings()
{
    for (int i = 0; i < (int)m_Things.size(); ++i)
    {
        Thing& thing = m_Things[i];
        if (thing.removed)
            continue;

        thing.stateTime += TICK;

        if (thing.type == ThingType::Barrel)
        {
            if (thing.state == ThingState::Dying && thing.stateTime >= BARREL_FUSE)
                explodeBarrel(thing);
        }
        else if (isMonster(thing.type))
        {
            updateMonster(i);
        }
    }
}

void Game::updateMonster(int index)
{
    Thing& monster = m_Things[index];
    const MonsterInfo& info = monsterInfo(monster.type);

    switch (monster.state)
    {
        case ThingState::Idle:
            lookForPlayer(monster);
            break;
        case ThingState::Chase:
            chase(index);
            break;
        case ThingState::Attack:
            attack(index);
            break;
        case ThingState::Pain:
            if (monster.stateTime >= info.painTime)
            {
                monster.state = ThingState::Chase;
                monster.stateTime = 0.f;
            }
            break;
        case ThingState::Dying:
            if (monster.stateTime >= info.deathTime)
                monster.state = ThingState::Dead;
            break;
        case ThingState::Dead:
            break;
    }
}

void Game::lookForPlayer(Thing& monster)
{
    const Player& player = m_Player;
    if (player.dead)
        return;

    const float dx = player.x - monster.x, dy = player.y - monster.y;
    const float distance = std::hypot(dx, dy);
    if (distance > SIGHT_RANGE)
        return;

    // Monsters see what is in front of them, and feel what is right behind.
    const float off = std::fabs(wrapAngle(std::atan2(dy, dx) - monster.angle));
    if (off > SIGHT_HALF_ANGLE && distance > 1.5f)
        return;

    if (!lineOfSight(m_Map, monster.x, monster.y, player.x, player.y))
        return;

    monster.target = PLAYER;
    wake(monster, true);
}

void Game::wake(Thing& monster, bool alertSound)
{
    if (monster.state != ThingState::Idle)
        return;

    monster.state = ThingState::Chase;
    monster.stateTime = 0.f;
    // A moment to react before the first attack.
    monster.attackCooldown = randomFloat(0.4f, 1.f);
    if (alertSound)
        playSound(Sound::MonsterAlert, monster.x, monster.y, monster.type);
}

void Game::chase(int index)
{
    Thing& monster = m_Things[index];
    const MonsterInfo& info = monsterInfo(monster.type);

    if (!isAlive(monster.target))
        monster.target = PLAYER;
    if (monster.target == PLAYER && m_Player.dead)
    {
        // Nothing left to fight.
        monster.state = ThingState::Idle;
        return;
    }

    const float tx = targetX(monster.target), ty = targetY(monster.target);
    const float dx = tx - monster.x, dy = ty - monster.y;
    const float distance = std::max(std::hypot(dx, dy), 0.001f);
    const float gap = distance - thingInfo(monster.type).radius - targetRadius(monster.target);
    const bool visible = lineOfSight(m_Map, monster.x, monster.y, tx, ty);

    if (visible)
        monster.angle = std::atan2(dy, dx);

    monster.attackCooldown -= TICK;
    if (visible && monster.attackCooldown <= 0.f)
    {
        const bool inReach = info.meleeRange > 0.f && gap <= info.meleeRange;
        // Ranged attacks come at random, more often up close.
        const bool shoot = info.ranged && chance(TICK * (distance < 8.f ? 1.6f : 0.7f));
        if (inReach || shoot)
        {
            monster.state = ThingState::Attack;
            monster.stateTime = 0.f;
            monster.struck = false;
            return;
        }
    }

    float mx = dx / distance, my = dy / distance;
    if (visible)
    {
        // Weave from side to side, harder to hit, except demons, which
        // charge straight in.
        if (monster.type != ThingType::Demon)
        {
            const float weave = std::sin(monster.walkTime * 1.3f + (float)index) * 0.7f;
            const float c = std::cos(weave), s = std::sin(weave);
            const float rx = mx * c - my * s;
            my = mx * s + my * c;
            mx = rx;
        }
    }
    else if (monster.target == PLAYER)
    {
        pathDirection(monster, mx, my);
    }

    if (gap > 0.05f)
        moveMonster(index, mx, my, info.speed * TICK);
}

/// Which way to step to get closer to the player, round walls and through
/// doors, by the map of steps to the player. False, leaving (dx, dy) alone,
/// if no neighbouring cell is any closer.
bool Game::pathDirection(const Thing& monster, float& dx, float& dy) const
{
    const int width = m_Map.width();
    const int x = (int)monster.x, y = (int)monster.y;
    auto steps = [&](int cx, int cy) {
        return m_Map.inside(cx, cy) ? m_Paths[(std::size_t)cy * width + cx] : -1;
    };

    int best = steps(x, y);
    if (best < 0)
        return false;

    int bestX = x, bestY = y;
    for (int oy = -1; oy <= 1; ++oy)
        for (int ox = -1; ox <= 1; ++ox)
        {
            const int value = steps(x + ox, y + oy);
            if ((ox == 0 && oy == 0) || value < 0 || value >= best)
                continue;
            // No cutting corners.
            if (ox != 0 && oy != 0 && (steps(x + ox, y) < 0 || steps(x, y + oy) < 0))
                continue;
            best = value;
            bestX = x + ox;
            bestY = y + oy;
        }

    if (bestX == x && bestY == y)
        return false;

    const float tx = (float)bestX + 0.5f - monster.x, ty = (float)bestY + 0.5f - monster.y;
    const float length = std::max(std::hypot(tx, ty), 0.001f);
    dx = tx / length;
    dy = ty / length;
    return true;
}

void Game::moveMonster(int index, float dx, float dy, float distance)
{
    Thing& monster = m_Things[index];
    const float radius = thingInfo(monster.type).radius;
    const float x = monster.x + dx * distance, y = monster.y + dy * distance;

    bool moved = true;
    if (canMove(index, monster.x, monster.y, x, y, radius))
    {
        monster.x = x;
        monster.y = y;
    }
    else if (std::fabs(dx) > 0.2f && canMove(index, monster.x, monster.y, x, monster.y, radius))
    {
        monster.x = x;
    }
    else if (std::fabs(dy) > 0.2f && canMove(index, monster.x, monster.y, monster.x, y, radius))
    {
        monster.y = y;
    }
    else
    {
        moved = false;
        // Monsters open doors too.
        const int aheadX = (int)std::floor(monster.x + dx * (radius + 0.3f));
        const int aheadY = (int)std::floor(monster.y + dy * (radius + 0.3f));
        if (m_Map.inside(aheadX, aheadY) && m_Map.at(aheadX, aheadY).tile == Tile::Door)
            openDoor(aheadX, aheadY);

        // Otherwise step aside, round whatever is in the way.
        const float sideX = -dy * monster.dodge * distance, sideY = dx * monster.dodge * distance;
        if (canMove(index, monster.x, monster.y, monster.x + sideX, monster.y + sideY, radius))
        {
            monster.x += sideX;
            monster.y += sideY;
            moved = true;
        }

        monster.stuckTime += TICK;
        if (monster.stuckTime > 0.6f)
        {
            monster.dodge = -monster.dodge;
            monster.stuckTime = 0.f;
        }
    }

    if (moved)
        monster.walkTime += TICK;
}

void Game::attack(int index)
{
    Thing& monster = m_Things[index];
    const MonsterInfo& info = monsterInfo(monster.type);

    if (!isAlive(monster.target))
    {
        monster.state = ThingState::Chase;
        monster.stateTime = 0.f;
        return;
    }

    monster.angle = std::atan2(targetY(monster.target) - monster.y, targetX(monster.target) - monster.x);

    if (!monster.struck && monster.stateTime >= info.windup)
    {
        monster.struck = true;
        strike(index);
    }

    if (monster.stateTime >= info.attackTime)
    {
        monster.state = ThingState::Chase;
        monster.stateTime = 0.f;
        monster.attackCooldown = randomFloat(info.cooldownMin, info.cooldownMax);
    }
}

void Game::strike(int index)
{
    const Thing& monster = m_Things[index];
    const MonsterInfo& info = monsterInfo(monster.type);
    const int target = monster.target;
    const float tx = targetX(target), ty = targetY(target);
    const float angle = std::atan2(ty - monster.y, tx - monster.x);
    const float gap = std::hypot(tx - monster.x, ty - monster.y) - thingInfo(monster.type).radius
                      - targetRadius(target);
    // A little slack, since the target may have stepped back mid swing.
    const bool close = info.meleeRange > 0.f && gap <= info.meleeRange + 0.15f;
    const float x = monster.x, y = monster.y;
    const ThingType type = monster.type;

    switch (type)
    {
        case ThingType::Trooper:
            playSound(Sound::TrooperShot, x, y, type);
            // Troopers are poor shots, as in Doom.
            hitscan(index, x, y, angle + randomFloat(-0.3f, 0.3f), 40.f, 3 * roll(1, 5));
            break;
        case ThingType::Imp:
            if (close)
            {
                playSound(Sound::Claw, x, y, type);
                damage(target, 3 * roll(1, 8), index);
            }
            else
            {
                playSound(Sound::FireballLaunch, x, y, type);
                spawnProjectile(ProjectileType::Fireball, index, x, y, angle);
            }
            break;
        case ThingType::Demon:
            playSound(Sound::Bite, x, y, type);
            if (close)
                damage(target, 4 * roll(1, 10), index);
            break;
        case ThingType::Knight:
            if (close)
            {
                playSound(Sound::Claw, x, y, type);
                damage(target, 10 * roll(1, 8), index);
            }
            else
            {
                playSound(Sound::PlasmaLaunch, x, y, type);
                spawnProjectile(ProjectileType::Plasma, index, x, y, angle);
            }
            break;
        default:
            break;
    }
}

void Game::killMonster(Thing& monster)
{
    monster.state = ThingState::Dying;
    monster.stateTime = 0.f;
    ++m_Stats.kills;
    playSound(Sound::MonsterDeath, monster.x, monster.y, monster.type);

    // Troopers drop their ammo.
    if (monster.type == ThingType::Trooper)
    {
        Thing clip{ThingType::Clip, monster.x, monster.y};
        clip.dropped = true;
        m_NewThings.push_back(clip);
    }
}

void Game::explodeBarrel(Thing& barrel)
{
    // Gone first, so the blast does not set it off again.
    barrel.removed = true;
    addEffect(EffectType::Explosion, barrel.x, barrel.y, 0.f);
    playSound(Sound::Explosion, barrel.x, barrel.y);
    radiusDamage(barrel.x, barrel.y, BLAST_DAMAGE, BLAST_RADIUS, barrel.target);
}

void Game::makeNoise(float x, float y)
{
    const int width = m_Map.width();
    std::vector<int> steps((std::size_t)width * m_Map.height(), -1);
    std::vector<int> queue;

    const int start = (int)y * width + (int)x;
    steps[start] = 0;
    queue.push_back(start);

    for (std::size_t head = 0; head < queue.size(); ++head)
    {
        const int cell = queue[head];
        if (steps[cell] >= HEARING_STEPS)
            continue;

        const int cx = cell % width, cy = cell / width;
        for (auto [ox, oy]: {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
        {
            const int nx = cx + ox, ny = cy + oy;
            if (!m_Map.inside(nx, ny) || m_Map.isWall(nx, ny))
                continue;
            // Shut doors keep the sound in.
            if (m_Map.at(nx, ny).tile == Tile::Door && m_Map.at(nx, ny).open < 0.5f)
                continue;

            const int next = ny * width + nx;
            if (steps[next] >= 0)
                continue;
            steps[next] = steps[cell] + 1;
            queue.push_back(next);
        }
    }

    for (Thing& thing: m_Things)
    {
        if (!isMonster(thing.type) || thing.state != ThingState::Idle)
            continue;
        if (steps[(std::size_t)((int)thing.y * width + (int)thing.x)] >= 0)
        {
            thing.target = PLAYER;
            wake(thing, true);
        }
    }
}

void Game::updatePathing()
{
    m_PathAge += TICK;
    const int x = (int)m_Player.x, y = (int)m_Player.y;
    // Doors open and shut, so refresh now and then even standing still.
    if (x == m_PathCellX && y == m_PathCellY && m_PathAge < 0.5f)
        return;

    m_PathCellX = x;
    m_PathCellY = y;
    m_PathAge = 0.f;

    const int width = m_Map.width();
    m_Paths.assign((std::size_t)width * m_Map.height(), -1);
    if (!m_Map.inside(x, y))
        return;

    std::vector<int> queue;
    m_Paths[(std::size_t)y * width + x] = 0;
    queue.push_back(y * width + x);

    for (std::size_t head = 0; head < queue.size(); ++head)
    {
        const int cell = queue[head];
        const int cx = cell % width, cy = cell / width;
        for (auto [ox, oy]: {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
        {
            const int nx = cx + ox, ny = cy + oy;
            // Doors count as open: monsters open them as they come.
            if (m_Map.isWall(nx, ny))
                continue;
            const int next = ny * width + nx;
            if (m_Paths[next] >= 0)
                continue;
            m_Paths[next] = m_Paths[cell] + 1;
            queue.push_back(next);
        }
    }
}

// --------------------------------------------------- shots and explosions --

int Game::hitscan(int shooter, float x, float y, float angle, float range, int amount)
{
    const float dx = std::cos(angle), dy = std::sin(angle);
    const RayHit wall = castRay(m_Map, x, y, dx, dy, range);

    float nearest = wall.hit ? wall.t : range;
    int victim = WORLD;

    // Where the ray first touches a circle, if before anything else.
    auto consider = [&](int index, float cx, float cy, float radius) {
        const float ox = x - cx, oy = y - cy;
        const float b = ox * dx + oy * dy;
        const float c = ox * ox + oy * oy - radius * radius;
        const float discriminant = b * b - c;
        if (discriminant < 0.f)
            return;
        const float root = std::sqrt(discriminant);
        float t = -b - root;
        if (t < 0.f)
        {
            // Starting inside counts, behind does not.
            if (-b + root < 0.f)
                return;
            t = 0.f;
        }
        if (t < nearest)
        {
            nearest = t;
            victim = index;
        }
    };

    for (int i = 0; i < (int)m_Things.size(); ++i)
        if (i != shooter && isShootable(m_Things[i]))
            consider(i, m_Things[i].x, m_Things[i].y, thingInfo(m_Things[i].type).radius);
    if (shooter != PLAYER && !m_Player.dead)
        consider(PLAYER, m_Player.x, m_Player.y, PLAYER_RADIUS);

    // Just short of the hit, so the effect is not inside the wall.
    const float hx = x + dx * (nearest - 0.05f), hy = y + dy * (nearest - 0.05f);
    const float height = shooter == PLAYER ? 0.42f : 0.35f;

    if (victim >= 0)
    {
        const bool bleeds = isMonster(m_Things[victim].type);
        addEffect(bleeds ? EffectType::Blood : EffectType::Puff, hx, hy, height);
        damage(victim, amount, shooter);
    }
    else if (victim == PLAYER)
    {
        damage(PLAYER, amount, shooter);
    }
    else if (wall.hit)
    {
        addEffect(EffectType::Puff, hx, hy, height);
    }
    return victim;
}

void Game::damage(int target, int amount, int source)
{
    if (target == PLAYER)
    {
        damagePlayer(amount, source);
        return;
    }
    if (target < 0 || target >= (int)m_Things.size())
        return;

    Thing& thing = m_Things[target];
    if (!isShootable(thing))
        return;

    thing.health -= amount;

    if (thing.type == ThingType::Barrel)
    {
        if (thing.health <= 0)
        {
            thing.state = ThingState::Dying;
            thing.stateTime = 0.f;
            // Whoever set it off is to blame for the blast.
            thing.target = source;
        }
        return;
    }

    if (thing.health <= 0)
    {
        killMonster(thing);
        return;
    }

    // Monsters turn on whatever hurt them, the player or another monster,
    // as long as it is not one of their own kind.
    if (source == PLAYER)
        thing.target = PLAYER;
    else if (source >= 0 && source != target && isMonster(m_Things[source].type)
             && m_Things[source].type != thing.type)
        thing.target = source;

    wake(thing, false);

    if (chance(monsterInfo(thing.type).painChance))
    {
        thing.state = ThingState::Pain;
        thing.stateTime = 0.f;
        playSound(Sound::MonsterPain, thing.x, thing.y, thing.type);
    }
}

void Game::radiusDamage(float x, float y, int maximum, float radius, int source)
{
    auto hurt = [&](int target, float tx, float ty, float targetRadius) {
        const float distance = std::max(0.f, std::hypot(tx - x, ty - y) - targetRadius);
        if (distance >= radius || !lineOfSight(m_Map, x, y, tx, ty))
            return;
        damage(target, (int)((float)maximum * (1.f - distance / radius)), source);
    };

    for (int i = 0; i < (int)m_Things.size(); ++i)
        if (isShootable(m_Things[i]))
            hurt(i, m_Things[i].x, m_Things[i].y, thingInfo(m_Things[i].type).radius);
    if (!m_Player.dead)
        hurt(PLAYER, m_Player.x, m_Player.y, PLAYER_RADIUS);
}

void Game::spawnProjectile(ProjectileType type, int owner, float x, float y, float angle)
{
    const float reach = (owner == PLAYER ? PLAYER_RADIUS : thingInfo(m_Things[owner].type).radius) + 0.1f;
    const float speed = projectileInfo(type).speed;
    const float dx = std::cos(angle), dy = std::sin(angle);
    m_Projectiles.push_back({type, x + dx * reach, y + dy * reach, dx * speed, dy * speed, owner});
}

void Game::updateProjectiles()
{
    // Several small steps a tick, so a fast rocket cannot skip over a monster.
    constexpr int SUBSTEPS = 4;
    constexpr float STEP = TICK / (float)SUBSTEPS;

    for (Projectile& projectile: m_Projectiles)
    {
        if (!projectile.alive)
            continue;

        projectile.age += TICK;
        for (int step = 0; step < SUBSTEPS && projectile.alive; ++step)
        {
            const float sx = projectile.vx * STEP, sy = projectile.vy * STEP;
            const RayHit wall = castRay(m_Map, projectile.x, projectile.y, sx, sy, 1.f);
            if (wall.hit && wall.t <= 1.f)
            {
                // Stop just short of the wall.
                const float t = std::max(0.f, wall.t - 0.1f / std::max(std::hypot(sx, sy), 1e-4f));
                projectile.x += sx * t;
                projectile.y += sy * t;
                burst(projectile, WORLD);
                break;
            }
            projectile.x += sx;
            projectile.y += sy;

            int victim = WORLD;
            for (int i = 0; i < (int)m_Things.size() && victim == WORLD; ++i)
            {
                const Thing& thing = m_Things[i];
                if (i == projectile.owner || !isShootable(thing))
                    continue;
                // Monsters' missiles pass through their own kind.
                if (projectile.owner >= 0 && m_Things[projectile.owner].type == thing.type)
                    continue;
                if (std::hypot(thing.x - projectile.x, thing.y - projectile.y)
                    < thingInfo(thing.type).radius + PROJECTILE_RADIUS)
                    victim = i;
            }
            if (victim == WORLD && projectile.owner != PLAYER && !m_Player.dead
                && std::hypot(m_Player.x - projectile.x, m_Player.y - projectile.y) < PLAYER_RADIUS + PROJECTILE_RADIUS)
                victim = PLAYER;

            if (victim != WORLD)
                burst(projectile, victim);
        }

        if (projectile.age > 10.f)
            projectile.alive = false;
    }

    std::erase_if(m_Projectiles, [](const Projectile& projectile) { return !projectile.alive; });
}

void Game::burst(Projectile& projectile, int victim)
{
    projectile.alive = false;
    if (victim != WORLD)
        damage(victim, projectileInfo(projectile.type).damage * roll(1, 8), projectile.owner);

    const float x = projectile.x, y = projectile.y;
    switch (projectile.type)
    {
        case ProjectileType::Rocket:
            addEffect(EffectType::Explosion, x, y, 0.f);
            playSound(Sound::Explosion, x, y);
            radiusDamage(x, y, BLAST_DAMAGE, BLAST_RADIUS, projectile.owner);
            break;
        case ProjectileType::Fireball:
            addEffect(EffectType::FireballBurst, x, y, 0.35f);
            playSound(Sound::FireballBurst, x, y);
            break;
        case ProjectileType::Plasma:
            addEffect(EffectType::PlasmaBurst, x, y, 0.35f);
            playSound(Sound::PlasmaBurst, x, y);
            break;
    }
}

void Game::updateEffects()
{
    for (Effect& effect: m_Effects)
        effect.age += TICK;
    std::erase_if(m_Effects, [](const Effect& effect) { return effect.age >= effectDuration(effect.type); });
}

void Game::addEffect(EffectType type, float x, float y, float z)
{
    m_Effects.push_back({type, x, y, z});
}

// --------------------------------------------------------------- helpers --

bool Game::wallBlocks(float x, float y, float radius) const
{
    for (int cy = (int)std::floor(y - radius); cy <= (int)std::floor(y + radius); ++cy)
        for (int cx = (int)std::floor(x - radius); cx <= (int)std::floor(x + radius); ++cx)
            if (m_Map.blocksMovement(cx, cy))
                return true;
    return false;
}

bool Game::canMove(int self, float fromX, float fromY, float x, float y, float radius) const
{
    if (wallBlocks(x, y, radius))
        return false;

    // Bodies may not move into one another, but may always move apart.
    auto blockedBy = [&](float ox, float oy, float otherRadius) {
        const float after = std::hypot(x - ox, y - oy);
        return after < radius + otherRadius && after < std::hypot(fromX - ox, fromY - oy);
    };

    for (int i = 0; i < (int)m_Things.size(); ++i)
        if (i != self && isSolid(m_Things[i])
            && blockedBy(m_Things[i].x, m_Things[i].y, thingInfo(m_Things[i].type).radius))
            return false;

    if (self != PLAYER && !m_Player.dead && blockedBy(m_Player.x, m_Player.y, PLAYER_RADIUS))
        return false;
    return true;
}

bool Game::isSolid(const Thing& thing) const
{
    if (thing.removed)
        return false;
    if (isMonster(thing.type))
        return thing.state != ThingState::Dying && thing.state != ThingState::Dead;
    return thing.type == ThingType::Barrel || thing.type == ThingType::Lamp || thing.type == ThingType::Column;
}

bool Game::isShootable(const Thing& thing) const
{
    if (thing.removed)
        return false;
    if (isMonster(thing.type))
        return thing.state != ThingState::Dying && thing.state != ThingState::Dead;
    return thing.type == ThingType::Barrel && thing.state != ThingState::Dying;
}

bool Game::isAlive(int target) const
{
    if (target == PLAYER)
        return !m_Player.dead;
    return target >= 0 && target < (int)m_Things.size() && isMonster(m_Things[target].type)
           && isShootable(m_Things[target]);
}

float Game::targetX(int target) const { return target == PLAYER ? m_Player.x : m_Things[target].x; }
float Game::targetY(int target) const { return target == PLAYER ? m_Player.y : m_Things[target].y; }

float Game::targetRadius(int target) const
{
    return target == PLAYER ? PLAYER_RADIUS : thingInfo(m_Things[target].type).radius;
}

void Game::playSound(Sound sound, float x, float y, ThingType source)
{
    m_Sounds.push_back({sound, source, x, y});
}

void Game::showMessage(const std::string& message)
{
    m_Message = message;
    m_MessageAge = 0.f;
}

int Game::roll(int low, int high)
{
    return std::uniform_int_distribution<int>(low, high)(m_Random);
}

float Game::randomFloat(float low, float high)
{
    return std::uniform_real_distribution<float>(low, high)(m_Random);
}

bool Game::chance(float probability)
{
    return randomFloat(0.f, 1.f) < probability;
}

} // namespace doom
