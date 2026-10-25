#ifndef MAPLEDOOM_FONT_H
#define MAPLEDOOM_FONT_H

#include "Rendering/Image.h"
#include <string_view>

/// A small blocky font, 5 x 7 pixels a letter, for the status bar,
/// messages and signs. Capital letters, digits and common punctuation:
/// lower case is drawn as upper case, anything else as a space.
namespace doom::font {

constexpr int GLYPH_WIDTH = 5;
constexpr int GLYPH_HEIGHT = 7;
/// From the start of one letter to the next, at scale 1.
constexpr int ADVANCE = 6;

int textWidth(std::string_view text, int scale = 1);

/// Draw text with its top left at (x, y), each font pixel `scale` pixels
/// square. Whatever falls outside the image is left off.
void drawText(Image& image, int x, int y, std::string_view text, std::uint32_t colour, int scale = 1);

/// The same, over a drop shadow one font pixel down and to the right.
void drawShadowedText(Image& image, int x, int y, std::string_view text, std::uint32_t colour, int scale = 1);

} // namespace doom::font

#endif //MAPLEDOOM_FONT_H
