#ifndef MAPLEENGINE_GLOW_H
#define MAPLEENGINE_GLOW_H

#include "../Component.h"
#include <glm/glm.hpp>
#include <optional>

/// Makes an entity give off light: its shape is drawn again into the glow
/// buffer, which the bloom pass blurs into a halo around it. Unlike bloom
/// from the scene, which only catches what is brighter than the threshold,
/// a glowing entity shines however dark it is drawn.
///
/// Needs bloom to be on, see World::setBloom().
class Glow : public Component {
public:
    /// Glow in the entity's own colour.
    explicit Glow(Entity& owner, float intensity = 1.f)
        : Component(owner), m_Intensity(intensity) {}

    /// Glow in a colour of its own, for a white-hot core with a coloured halo,
    /// say.
    Glow(Entity& owner, const glm::vec3& colour, float intensity = 1.f)
        : Component(owner), m_Colour(colour), m_Intensity(intensity) {}

    /// Values above 1 make the halo brighter and wider.
    void setIntensity(float intensity) { m_Intensity = intensity; }
    float getIntensity() const { return m_Intensity; }

    void setColour(const glm::vec3& colour) { m_Colour = colour; }
    /// Go back to glowing in the entity's own colour.
    void useEntityColour() { m_Colour.reset(); }
    const std::optional<glm::vec3>& getColour() const { return m_Colour; }

    void setEnabled(bool enabled) { m_Enabled = enabled; }
    bool isEnabled() const { return m_Enabled && m_Intensity > 0.f; }

private:
    std::optional<glm::vec3> m_Colour;
    float m_Intensity;
    bool m_Enabled = true;
};

#endif //MAPLEENGINE_GLOW_H
