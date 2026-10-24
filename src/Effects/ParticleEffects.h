#ifndef MAPLEENGINE_PARTICLEEFFECTS_H
#define MAPLEENGINE_PARTICLEEFFECTS_H

#include "ParticleSystem.h"

/// Ready-made particle effects. Each takes `size`, the width of the effect at
/// its base in world units, and scales everything else to match, so the same
/// fire works in a scene measured in metres and one measured in pixels.
///
/// The fire colours are well above 1, so they glow with bloom on.
namespace effects {

/// A steady flame, licking upwards and narrowing to a tip.
inline ParticleEffect fire(float size = 1.f)
{
    ParticleEffect e;
    e.rate = 90.f;
    e.maxParticles = 400;
    e.lifetimeMin = 0.45f;
    e.lifetimeMax = 0.85f;
    e.spawnRadius = 0.3f * size;
    e.spread = 0.5f;
    e.speedMin = 0.4f * size;
    e.speedMax = 1.2f * size;
    e.acceleration = {0.f, 5.f * size};
    e.drag = 1.5f;
    e.turbulence = 4.f * size;
    e.sizeStart = 0.9f * size;
    e.sizeEnd = 0.2f * size;
    e.softness = 1.4f;
    e.colourStart = {4.0f, 2.4f, 0.9f, 0.8f};
    e.colourMiddle = {3.0f, 0.9f, 0.15f, 0.6f};
    e.colourEnd = {0.8f, 0.12f, 0.02f, 0.f};
    e.blend = ParticleBlend::Additive;
    return e;
}

/// Grey smoke that rises slowly, spreads out and fades.
inline ParticleEffect smoke(float size = 1.f)
{
    ParticleEffect e;
    e.rate = 18.f;
    e.maxParticles = 300;
    e.lifetimeMin = 2.f;
    e.lifetimeMax = 3.5f;
    e.spawnRadius = 0.3f * size;
    e.spread = 0.6f;
    e.speedMin = 0.4f * size;
    e.speedMax = 0.9f * size;
    e.acceleration = {0.f, 0.6f * size};
    e.drag = 0.7f;
    e.turbulence = 1.5f * size;
    e.sizeStart = 0.6f * size;
    e.sizeEnd = 3.f * size;
    e.softness = 2.f;
    // Fades in as well as out, so puffs do not pop into existence.
    e.colourStart = {0.45f, 0.43f, 0.41f, 0.f};
    e.colourMiddle = {0.36f, 0.35f, 0.35f, 0.4f};
    e.colourEnd = {0.3f, 0.3f, 0.31f, 0.f};
    e.blend = ParticleBlend::Alpha;
    return e;
}

/// Streaks that fly out in every direction and fall. For bursts.
inline ParticleEffect sparks(float size = 1.f)
{
    ParticleEffect e;
    e.rate = 20.f;
    e.maxParticles = 1500;
    e.lifetimeMin = 0.4f;
    e.lifetimeMax = 1.2f;
    e.spread = 6.2831853f;
    e.speedMin = 4.f * size;
    e.speedMax = 12.f * size;
    e.acceleration = {0.f, -9.f * size};
    e.drag = 1.2f;
    e.sizeStart = 0.15f * size;
    e.sizeEnd = 0.08f * size;
    e.stretch = 0.04f;
    e.softness = 0.8f;
    e.colourStart = {6.f, 3.f, 1.f, 1.f};
    e.colourMiddle = {3.f, 0.9f, 0.15f, 1.f};
    e.colourEnd = {1.f, 0.1f, 0.f, 0.f};
    e.blend = ParticleBlend::Additive;
    return e;
}

/// Small glowing bits that drift up from a fire.
inline ParticleEffect embers(float size = 1.f)
{
    ParticleEffect e;
    e.rate = 6.f;
    e.maxParticles = 300;
    e.lifetimeMin = 1.f;
    e.lifetimeMax = 2.2f;
    e.spawnRadius = 0.4f * size;
    e.spread = 0.8f;
    e.speedMin = 1.f * size;
    e.speedMax = 2.5f * size;
    e.acceleration = {0.f, 0.8f * size};
    e.drag = 0.6f;
    e.turbulence = 3.f * size;
    e.sizeStart = 0.12f * size;
    e.sizeEnd = 0.05f * size;
    e.softness = 0.8f;
    e.colourStart = {5.f, 2.f, 0.5f, 1.f};
    e.colourMiddle = {3.f, 0.8f, 0.1f, 0.9f};
    e.colourEnd = {1.f, 0.1f, 0.f, 0.f};
    e.blend = ParticleBlend::Additive;
    return e;
}

/// A fireball that bursts outwards and burns out quickly. For bursts.
inline ParticleEffect explosion(float size = 1.f)
{
    ParticleEffect e;
    e.rate = 0.f;
    e.maxParticles = 600;
    e.lifetimeMin = 0.25f;
    e.lifetimeMax = 0.6f;
    e.spawnRadius = 0.2f * size;
    e.spread = 6.2831853f;
    e.speedMin = 0.5f * size;
    e.speedMax = 3.f * size;
    e.acceleration = {0.f, 1.5f * size};
    e.drag = 3.f;
    e.turbulence = 2.f * size;
    e.sizeStart = 0.8f * size;
    e.sizeEnd = 0.3f * size;
    e.softness = 1.2f;
    e.colourStart = {8.f, 5.f, 2.f, 0.9f};
    e.colourMiddle = {4.f, 1.2f, 0.2f, 0.6f};
    e.colourEnd = {0.6f, 0.1f, 0.02f, 0.f};
    e.blend = ParticleBlend::Additive;
    return e;
}

} // namespace effects

#endif //MAPLEENGINE_PARTICLEEFFECTS_H
