#include "ParticleSystem.h"
#include "../Rendering/ShaderProgram.h"
#include <GL/glew.h>
#include <algorithm>
#include <cmath>

namespace {

/// Draws each quad as a round, soft-edged blob. GLSL 1.20 to read the matrices
/// World::applyView set, and the colour comes through a varying of its own,
/// so that colours above 1 reach the bloom pass unclamped.
constexpr const char* PARTICLE_VERTEX = R"(
#version 120
varying vec2 v_UV;
varying vec4 v_Colour;

void main()
{
    v_UV = gl_MultiTexCoord0.xy;
    v_Colour = gl_Color;
    gl_Position = ftransform();
}
)";

constexpr const char* PARTICLE_FRAGMENT = R"(
#version 120
varying vec2 v_UV;
varying vec4 v_Colour;
uniform float u_Softness;

void main()
{
    float distance = length(v_UV * 2.0 - 1.0);
    float shape = pow(clamp(1.0 - distance, 0.0, 1.0), u_Softness);
    gl_FragColor = vec4(v_Colour.rgb, v_Colour.a * shape);
}
)";

/// Built on first use and kept for the life of the program: particle systems
/// come and go, and freeing it after the context is gone would be worse than
/// never freeing it.
GLuint particleProgram()
{
    static GLuint program = linkProgram(PARTICLE_VERTEX, PARTICLE_FRAGMENT, "particle");
    return program;
}

glm::vec4 colourAt(const ParticleEffect& effect, float t)
{
    if (t < 0.5f)
        return glm::mix(effect.colourStart, effect.colourMiddle, t * 2.f);
    return glm::mix(effect.colourMiddle, effect.colourEnd, (t - 0.5f) * 2.f);
}

} // namespace

ParticleSystem::ParticleSystem(const ParticleEffect& effect, std::uint32_t seed)
    : m_Effect(effect), m_Random(seed)
{}

float ParticleSystem::random(float min, float max)
{
    return std::uniform_real_distribution<float>(min, std::max(min, max))(m_Random);
}

void ParticleSystem::emit(glm::vec2 position, float deltaTime)
{
    m_EmitCarry += m_Effect.rate * deltaTime;
    const int count = (int)m_EmitCarry;
    m_EmitCarry -= (float)count;
    burst(position, count);
}

void ParticleSystem::burst(glm::vec2 position, int count)
{
    for (int i = 0; i < count && (int)m_Particles.size() < m_Effect.maxParticles; ++i)
        spawn(position);
}

void ParticleSystem::spawn(glm::vec2 position)
{
    const float spawnAngle = random(0.f, 6.2831853f);
    // sqrt spreads them evenly over the disc rather than bunching them in
    // the middle.
    const float spawnDistance = m_Effect.spawnRadius * std::sqrt(random(0.f, 1.f));
    const float angle = m_Effect.direction + random(-0.5f, 0.5f) * m_Effect.spread;
    const float speed = random(m_Effect.speedMin, m_Effect.speedMax);

    m_Particles.push_back(Particle{
            position + spawnDistance * glm::vec2{std::cos(spawnAngle), std::sin(spawnAngle)},
            speed * glm::vec2{std::cos(angle), std::sin(angle)},
            0.f,
            random(m_Effect.lifetimeMin, m_Effect.lifetimeMax),
            random(1.f - m_Effect.sizeVariation, 1.f + m_Effect.sizeVariation)});
}

void ParticleSystem::update(float deltaTime)
{
    if (deltaTime <= 0.f)
        return;

    const float damping = std::max(0.f, 1.f - m_Effect.drag * deltaTime);

    for (Particle& particle: m_Particles)
    {
        glm::vec2 push{0.f};
        if (m_Effect.turbulence > 0.f)
            push = m_Effect.turbulence * glm::vec2{random(-1.f, 1.f), random(-1.f, 1.f)};

        particle.velocity += (m_Effect.acceleration + push) * deltaTime;
        particle.velocity *= damping;
        particle.position += particle.velocity * deltaTime;
        particle.age += deltaTime;
    }

    std::erase_if(m_Particles, [](const Particle& particle) { return particle.age >= particle.lifetime; });
}

void ParticleSystem::render() const
{
    const GLuint program = particleProgram();
    if (m_Particles.empty() || program == 0)
        return;

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    if (m_Effect.blend == ParticleBlend::Additive)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program);
    glUniform1f(glGetUniformLocation(program, "u_Softness"), m_Effect.softness);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_QUADS);
    for (const Particle& particle: m_Particles)
    {
        const float t = particle.age / particle.lifetime;
        const glm::vec4 colour = colourAt(m_Effect, t);
        const float size = glm::mix(m_Effect.sizeStart, m_Effect.sizeEnd, t) * particle.scale;

        // Half extents along the particle's own axes: across, and along its
        // direction of travel when stretched.
        glm::vec2 along{size / 2.f, 0.f};
        glm::vec2 across{0.f, size / 2.f};
        const float speed = glm::length(particle.velocity);
        if (m_Effect.stretch > 0.f && speed > 1e-4f)
        {
            const glm::vec2 direction = particle.velocity / speed;
            along = direction * (size + speed * m_Effect.stretch) / 2.f;
            across = glm::vec2{-direction.y, direction.x} * size / 2.f;
        }

        const glm::vec2 p = particle.position;
        glColor4f(colour.r, colour.g, colour.b, colour.a);
        glTexCoord2f(0.f, 0.f); glVertex2f(p.x - along.x - across.x, p.y - along.y - across.y);
        glTexCoord2f(1.f, 0.f); glVertex2f(p.x + along.x - across.x, p.y + along.y - across.y);
        glTexCoord2f(1.f, 1.f); glVertex2f(p.x + along.x + across.x, p.y + along.y + across.y);
        glTexCoord2f(0.f, 1.f); glVertex2f(p.x - along.x + across.x, p.y - along.y + across.y);
    }
    glEnd();

    glUseProgram(0);
    glPopAttrib();
}
