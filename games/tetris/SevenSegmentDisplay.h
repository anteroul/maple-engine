#ifndef MAPLETETRIS_SEVENSEGMENTDISPLAY_H
#define MAPLETETRIS_SEVENSEGMENTDISPLAY_H

#include "ECS/Components/BoxRenderer.h"
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

/// A row of seven segment digits, each segment a BoxRenderer. The engine has
/// no text rendering, so this is how numbers get on screen.
class SevenSegmentDisplay {
public:
    /// Segments of one digit, in the conventional order: a top, b top right,
    /// c bottom right, d bottom, e bottom left, f top left, g middle.
    using Digit = std::array<BoxRenderer*, 7>;

    SevenSegmentDisplay() = default;
    /// \param digits Most significant digit first.
    SevenSegmentDisplay(std::vector<Digit> digits, glm::vec3 lit, glm::vec3 unlit)
        : m_Digits(std::move(digits)), m_Lit(lit), m_Unlit(unlit) {}

    /// Show `value`, zero padded to the display's width. A value too wide for
    /// the display shows as all nines.
    void setValue(std::uint32_t value)
    {
        std::uint32_t limit = 1;
        for (size_t i = 0; i < m_Digits.size(); ++i)
            limit *= 10;
        if (value >= limit)
            value = limit - 1;

        for (size_t i = m_Digits.size(); i-- > 0;)
        {
            const std::uint8_t mask = SEGMENTS[value % 10];
            value /= 10;

            for (int segment = 0; segment < 7; ++segment)
                m_Digits[i][segment]->setColour((mask >> segment) & 1 ? m_Lit : m_Unlit);
        }
    }

private:
    /// Bit n lights segment n (a = bit 0 ... g = bit 6), for digits 0 to 9.
    static constexpr std::uint8_t SEGMENTS[10] = {
            0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
            0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};

    std::vector<Digit> m_Digits;
    glm::vec3 m_Lit{1.f};
    glm::vec3 m_Unlit{0.f};
};

#endif //MAPLETETRIS_SEVENSEGMENTDISPLAY_H
