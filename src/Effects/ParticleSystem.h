#ifndef MAPLEENGINE_PARTICLESYSTEM_H
#define MAPLEENGINE_PARTICLESYSTEM_H

#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

/// How particles are drawn over what is already there.
enum class ParticleBlend {
    /// Light adds up: fire, sparks, magic.
    Additive,
    /// Particles cover what is behind them: smoke, dust.
    Alpha
};

/// Everything that makes one kind of particle look and move the way it does.
/// The presets in Effects/ParticleEffects.h are good starting points.
///
/// Distances are world units and times are seconds. Colours may go above 1:
/// with bloom on, those glow.
struct ParticleEffect {
    /// Particles a second while emitting continuously.
    float rate = 30.f;
    /// Upper bound on live particles; emission pauses at it.
    int maxParticles = 500;

    float lifetimeMin = 1.f;
    float lifetimeMax = 1.f;

    /// Particles start anywhere within this radius of the emission point.
    float spawnRadius = 0.f;
    /// Launch direction in radians, 0 along +x and pi/2 straight up, and the
    /// full width of the cone around it. 2 pi sprays in every direction.
    float direction = 1.5707964f;
    float spread = 0.f;
    float speedMin = 0.f;
    float speedMax = 0.f;

    /// Constant acceleration: up for buoyant flames and smoke, down for sparks.
    glm::vec2 acceleration{0.f, 0.f};
    /// Fraction of velocity lost per second.
    float drag = 0.f;
    /// Strength of the random push each particle gets every frame, which is
    /// what makes flames lick and smoke curl.
    float turbulence = 0.f;

    float sizeStart = 1.f;
    float sizeEnd = 1.f;
    /// Each particle's size is scaled by a random factor in [1 - v, 1 + v].
    float sizeVariation = 0.25f;
    /// Above 0, particles stretch along their velocity by this many seconds
    /// of travel. Streaks for sparks.
    float stretch = 0.f;
    /// Edge falloff of each particle's round shape: 1 is a soft blob, higher
    /// concentrates it towards the middle.
    float softness = 1.f;

    /// Colour over a particle's life: start, middle and end. Alpha is opacity
    /// in Alpha blending and strength in Additive.
    glm::vec4 colourStart{1.f};
    glm::vec4 colourMiddle{1.f};
    glm::vec4 colourEnd{1.f, 1.f, 1.f, 0.f};

    ParticleBlend blend = ParticleBlend::Additive;
};

/// A pool of particles in world space. Particles stay where they were emitted
/// rather than following the emitter, which is what makes a moving torch
/// leave a trail of flame.
///
/// Use it directly from a component that draws several effects, or through
/// the ParticleEmitter component to attach one to an entity.
///
/// Where compute shaders are available (OpenGL 4.3), particles live on the
/// GPU: the CPU only works out where new ones start, and a compute shader
/// steps them all and builds the quads they are drawn with. Elsewhere they
/// are stepped and drawn on the CPU. Both look the same.
class ParticleSystem {
public:
    explicit ParticleSystem(const ParticleEffect& effect = ParticleEffect{},
                            std::uint32_t seed = std::random_device{}());
    ~ParticleSystem();
    // Owns GL buffers, so it can be moved but not copied.
    ParticleSystem(ParticleSystem&&) noexcept;
    ParticleSystem& operator=(ParticleSystem&&) noexcept;

    /// Takes effect from the next update. Changing maxParticles clears the
    /// particles on the GPU, which has to make room for the new number.
    void setEffect(const ParticleEffect& effect) { m_Effect = effect; }
    const ParticleEffect& getEffect() const { return m_Effect; }

    /// Emit at the effect's rate for one frame of deltaTime seconds.
    void emit(glm::vec2 position, float deltaTime);
    /// Emit `count` particles at once.
    void burst(glm::vec2 position, int count);
    /// Age, move and retire particles.
    void update(float deltaTime);
    /// Draw with the current projection, over whatever is already drawn.
    void render() const;

    void clear();
    /// Live particles.
    std::size_t size() const;

private:
    /// Laid out as the compute shader reads it (std430), so the GPU path can
    /// upload new particles as they are.
    struct Particle {
        glm::vec2 position;
        glm::vec2 velocity;
        float age;
        float lifetime;
        float scale;
        /// Picks the particle's turbulence on the GPU.
        std::uint32_t seed;
    };
    static_assert(sizeof(Particle) == 32, "must match struct Particle in src/Shaders/particles.comp");

    struct Gpu;

    /// Decides, on first use, whether particles go to the GPU.
    bool onGpu() const;
    Particle makeParticle(glm::vec2 position);
    float random(float min, float max);

    void updateOnCpu(float deltaTime);
    void renderOnCpu() const;

    ParticleEffect m_Effect;
    /// The particles, when they are on the CPU.
    std::vector<Particle> m_Particles;
    /// Everything for the GPU path. Empty until the first use decides.
    mutable std::unique_ptr<Gpu> m_Gpu;
    mutable bool m_Decided = false;
    /// Fraction of a particle left over from the last emit(), so that low
    /// rates at high frame rates still emit.
    float m_EmitCarry = 0.f;
    std::mt19937 m_Random;
};

#endif //MAPLEENGINE_PARTICLESYSTEM_H
