#ifndef MAPLEENGINE_PARTICLEEMITTER_H
#define MAPLEENGINE_PARTICLEEMITTER_H

#include "../Component.h"
#include "../../Effects/ParticleSystem.h"

/// Emits particles from an entity: a torch, a burning crate, an engine
/// exhaust. Emission follows the entity, but particles already emitted stay
/// where they are, so a moving emitter leaves a trail.
///
///     entity->addComponent(new ParticleEmitter(*entity, effects::smoke(0.5f)));
///     entity->addComponent(new ParticleEmitter(*entity, effects::fire(0.5f)));
///
/// Components draw in the order they were added, so add smoke before fire to
/// have the flame show through it.
class ParticleEmitter : public Component {
public:
    ParticleEmitter(Entity& owner, const ParticleEffect& effect, glm::vec2 offset = {0.f, 0.f})
        : Component(owner), m_System(effect), m_Offset(offset) {}

    void update(GLFWwindow*, float deltaTime) override
    {
        if (m_Emitting)
            m_System.emit(origin(), deltaTime);
        m_System.update(deltaTime);
    }

    void render() const override { m_System.render(); }

    /// Stop or restart continuous emission. Particles already out live on.
    void setEmitting(bool emitting) { m_Emitting = emitting; }
    bool isEmitting() const { return m_Emitting; }

    /// Emit `count` particles at once, on top of any continuous emission.
    void burst(int count) { m_System.burst(origin(), count); }

    /// Emission point relative to the entity's centre.
    void setOffset(glm::vec2 offset) { m_Offset = offset; }

    ParticleSystem& getSystem() { return m_System; }

private:
    glm::vec2 origin() const
    {
        const Transform& transform = getEntity().transform;
        return glm::vec2{transform.x, transform.y} + m_Offset;
    }

    ParticleSystem m_System;
    glm::vec2 m_Offset;
    bool m_Emitting = true;
};

#endif //MAPLEENGINE_PARTICLEEMITTER_H
