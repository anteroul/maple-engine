#include "ParticleSystem.h"
#include "../Rendering/ShaderProgram.h"
#include "Shaders/particles.comp.h"
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

/// The particle compute shader and where its uniforms are. Built on first
/// use and kept, like the particle drawing shader.
struct StepProgram {
    GLuint id = 0;
    GLint count, frame, deltaTime, acceleration, drag, turbulence;
    GLint sizeStart, sizeEnd, stretch, colourStart, colourMiddle, colourEnd;
};

const StepProgram& stepProgram()
{
    static const StepProgram program = []
    {
        StepProgram p;
        p.id = linkCompute(PARTICLES_COMP, "particle compute");
        if (p.id == 0)
            return p;
        const auto at = [&](const char* name) { return glGetUniformLocation(p.id, name); };
        p.count = at("u_Count");
        p.frame = at("u_Frame");
        p.deltaTime = at("u_DeltaTime");
        p.acceleration = at("u_Acceleration");
        p.drag = at("u_Drag");
        p.turbulence = at("u_Turbulence");
        p.sizeStart = at("u_SizeStart");
        p.sizeEnd = at("u_SizeEnd");
        p.stretch = at("u_Stretch");
        p.colourStart = at("u_ColourStart");
        p.colourMiddle = at("u_ColourMiddle");
        p.colourEnd = at("u_ColourEnd");
        return p;
    }();
    return program;
}

/// Matches struct Vertex in src/Shaders/particles.comp.
constexpr GLsizei VERTEX_SIZE = 32;
constexpr int VERTICES_PER_PARTICLE = 6;
constexpr GLuint GROUP_SIZE = 256;

} // namespace

/// The GPU side of a ParticleSystem: the particle and vertex buffers, plus
/// enough bookkeeping to spawn into free slots without reading anything back.
///
/// Slots are handed out in ring order. The CPU knows when each particle it
/// spawned will die, so it knows which slots are free; when the next slot in
/// the ring is still alive, emission pauses, as it does at maxParticles on
/// the CPU.
struct ParticleSystem::Gpu {
    GLuint particles = 0;
    GLuint vertices = 0;
    int capacity = 0;

    /// Seconds of simulated time, and when the particle in each slot dies.
    double clock = 0.0;
    std::vector<double> deathTime;
    /// When the last particle alive dies: past it, there is nothing to draw.
    double lastDeath = 0.0;
    int head = 0;

    /// Spawned since the last upload, for the slots from pendingStart on.
    std::vector<Particle> pending;
    int pendingStart = 0;
    std::uint32_t frame = 0;

    explicit Gpu(int slots) { allocate(slots); }

    ~Gpu()
    {
        // At exit the context can be gone before the last particle system.
        if (renderingReleased())
            return;
        glDeleteBuffers(1, &particles);
        glDeleteBuffers(1, &vertices);
    }

    void allocate(int slots)
    {
        capacity = std::max(slots, 1);
        if (!particles)
            glGenBuffers(1, &particles);
        if (!vertices)
            glGenBuffers(1, &vertices);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particles);
        glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)capacity * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertices);
        glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)capacity * VERTICES_PER_PARTICLE * VERTEX_SIZE,
                     nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        deathTime.assign(capacity, 0.0);
        clear();
    }

    void clear()
    {
        // All zero is a dead particle: its age has reached its lifetime of 0.
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particles);
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        std::fill(deathTime.begin(), deathTime.end(), clock);
        lastDeath = clock;
        head = 0;
        pending.clear();
        pendingStart = 0;
    }

    /// Claims the next slot, or returns false if it is still in use.
    bool spawn(const Particle& particle)
    {
        if (deathTime[head] > clock)
            return false;
        if (pending.empty())
            pendingStart = head;
        pending.push_back(particle);
        deathTime[head] = clock + particle.lifetime;
        lastDeath = std::max(lastDeath, deathTime[head]);
        head = (head + 1) % capacity;
        return true;
    }

    /// Sends what was spawned since the last upload: one run of slots, or
    /// two when it wraps round the end of the buffer.
    void upload()
    {
        if (pending.empty())
            return;
        const int count = (int)pending.size();
        const int first = std::min(count, capacity - pendingStart);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particles);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)pendingStart * sizeof(Particle),
                        (GLsizeiptr)first * sizeof(Particle), pending.data());
        if (count > first)
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(count - first) * sizeof(Particle),
                            pending.data() + first);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        pending.clear();
    }

    /// Steps every particle by deltaTime, 0 to only rebuild the quads, and
    /// leaves the quads ready to draw.
    void step(const ParticleEffect& effect, float deltaTime)
    {
        const StepProgram& program = stepProgram();
        upload();

        glUseProgram(program.id);
        glUniform1ui(program.count, (GLuint)capacity);
        glUniform1ui(program.frame, frame);
        glUniform1f(program.deltaTime, deltaTime);
        glUniform2f(program.acceleration, effect.acceleration.x, effect.acceleration.y);
        glUniform1f(program.drag, effect.drag);
        glUniform1f(program.turbulence, effect.turbulence);
        glUniform1f(program.sizeStart, effect.sizeStart);
        glUniform1f(program.sizeEnd, effect.sizeEnd);
        glUniform1f(program.stretch, effect.stretch);
        glUniform4fv(program.colourStart, 1, &effect.colourStart.x);
        glUniform4fv(program.colourMiddle, 1, &effect.colourMiddle.x);
        glUniform4fv(program.colourEnd, 1, &effect.colourEnd.x);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particles);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vertices);
        glDispatchCompute(((GLuint)capacity + GROUP_SIZE - 1) / GROUP_SIZE, 1, 1);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        glUseProgram(0);

        // The quads are read as vertices next, and the particles may be
        // overwritten by the next upload.
        glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT
                        | GL_SHADER_STORAGE_BARRIER_BIT);
        if (deltaTime > 0.f)
            frame++;
    }

    bool anyAlive() const { return lastDeath > clock; }
};

ParticleSystem::ParticleSystem(const ParticleEffect& effect, std::uint32_t seed)
    : m_Effect(effect), m_Random(seed)
{}

ParticleSystem::~ParticleSystem() = default;
ParticleSystem::ParticleSystem(ParticleSystem&&) noexcept = default;
ParticleSystem& ParticleSystem::operator=(ParticleSystem&&) noexcept = default;

bool ParticleSystem::onGpu() const
{
    if (!m_Decided)
    {
        m_Decided = true;
        if (computeSupported() && stepProgram().id != 0)
            m_Gpu = std::make_unique<Gpu>(m_Effect.maxParticles);
    }
    if (m_Gpu && m_Gpu->capacity != std::max(m_Effect.maxParticles, 1))
        m_Gpu->allocate(m_Effect.maxParticles);
    return m_Gpu != nullptr;
}

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
    if (onGpu())
    {
        for (int i = 0; i < count; ++i)
            if (!m_Gpu->spawn(makeParticle(position)))
                break;
        return;
    }

    for (int i = 0; i < count && (int)m_Particles.size() < m_Effect.maxParticles; ++i)
        m_Particles.push_back(makeParticle(position));
}

ParticleSystem::Particle ParticleSystem::makeParticle(glm::vec2 position)
{
    const float spawnAngle = random(0.f, 6.2831853f);
    // sqrt spreads them evenly over the disc rather than bunching them in
    // the middle.
    const float spawnDistance = m_Effect.spawnRadius * std::sqrt(random(0.f, 1.f));
    const float angle = m_Effect.direction + random(-0.5f, 0.5f) * m_Effect.spread;
    const float speed = random(m_Effect.speedMin, m_Effect.speedMax);

    return Particle{
            position + spawnDistance * glm::vec2{std::cos(spawnAngle), std::sin(spawnAngle)},
            speed * glm::vec2{std::cos(angle), std::sin(angle)},
            0.f,
            random(m_Effect.lifetimeMin, m_Effect.lifetimeMax),
            random(1.f - m_Effect.sizeVariation, 1.f + m_Effect.sizeVariation),
            (std::uint32_t)m_Random()};
}

void ParticleSystem::update(float deltaTime)
{
    if (deltaTime <= 0.f)
        return;

    if (!onGpu())
    {
        updateOnCpu(deltaTime);
        return;
    }

    m_Gpu->clock += deltaTime;
    // Nothing alive and nothing new: no need to wake the GPU.
    if (m_Gpu->anyAlive() || !m_Gpu->pending.empty())
        m_Gpu->step(m_Effect, deltaTime);
}

void ParticleSystem::updateOnCpu(float deltaTime)
{
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

void ParticleSystem::clear()
{
    m_Particles.clear();
    if (m_Gpu)
        m_Gpu->clear();
}

std::size_t ParticleSystem::size() const
{
    if (!m_Gpu)
        return m_Particles.size();
    return (std::size_t)std::count_if(m_Gpu->deathTime.begin(), m_Gpu->deathTime.end(),
                                      [&](double death) { return death > m_Gpu->clock; });
}

void ParticleSystem::render() const
{
    if (!onGpu())
    {
        renderOnCpu();
        return;
    }

    const GLuint program = particleProgram();
    if (program == 0)
        return;
    // Spawned since the last update: build their quads so they show now,
    // as they would on the CPU.
    if (!m_Gpu->pending.empty())
        m_Gpu->step(m_Effect, 0.f);
    if (!m_Gpu->anyAlive())
        return;

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glEnable(GL_BLEND);
    if (m_Effect.blend == ParticleBlend::Additive)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program);
    glUniform1f(glGetUniformLocation(program, "u_Softness"), m_Effect.softness);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // The quads the compute shader wrote, fed through the same attributes
    // glVertex, glTexCoord and glColor set on the CPU path.
    glBindBuffer(GL_ARRAY_BUFFER, m_Gpu->vertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, VERTEX_SIZE, (const void*)0);
    glTexCoordPointer(2, GL_FLOAT, VERTEX_SIZE, (const void*)8);
    glColorPointer(4, GL_FLOAT, VERTEX_SIZE, (const void*)16);
    glDrawArrays(GL_TRIANGLES, 0, m_Gpu->capacity * VERTICES_PER_PARTICLE);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(0);
    glPopClientAttrib();
    glPopAttrib();
}

void ParticleSystem::renderOnCpu() const
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
