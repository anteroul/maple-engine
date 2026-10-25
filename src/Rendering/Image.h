#ifndef MAPLEENGINE_IMAGE_H
#define MAPLEENGINE_IMAGE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// One 8 bit RGBA pixel packed into 32 bits: red in the lowest byte, alpha in
/// the highest. In memory the bytes then run R, G, B, A, which is what
/// GL_RGBA with GL_UNSIGNED_BYTE expects on the little endian machines the
/// engine runs on.
constexpr std::uint32_t rgba(int r, int g, int b, int a = 255)
{
    return (std::uint32_t)(r & 0xFF) | (std::uint32_t)(g & 0xFF) << 8
           | (std::uint32_t)(b & 0xFF) << 16 | (std::uint32_t)(a & 0xFF) << 24;
}

constexpr int redOf(std::uint32_t pixel) { return (int)(pixel & 0xFF); }
constexpr int greenOf(std::uint32_t pixel) { return (int)(pixel >> 8 & 0xFF); }
constexpr int blueOf(std::uint32_t pixel) { return (int)(pixel >> 16 & 0xFF); }
constexpr int alphaOf(std::uint32_t pixel) { return (int)(pixel >> 24); }

/// A picture in memory, for textures and sprites worked on by the CPU: a
/// software renderer's textures, or the frame it draws, shown with
/// PixelCanvas::upload().
///
/// Rows run from the top down, as they do in image files.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;

    Image() = default;
    Image(int width, int height, std::uint32_t fill = 0)
        : width(width), height(height), pixels((std::size_t)width * height, fill)
    {}

    bool empty() const { return pixels.empty(); }

    std::uint32_t& at(int x, int y) { return pixels[(std::size_t)y * width + x]; }
    std::uint32_t at(int x, int y) const { return pixels[(std::size_t)y * width + x]; }

    /// Load an image file: PNG, JPEG, BMP, TGA, GIF (the first frame) and
    /// more. Prints why and returns nothing if the file cannot be read.
    static std::optional<Image> load(const std::string& path);
    /// The same, from a file already in memory, such as one embedded in the
    /// executable. `name` only labels the warning.
    static std::optional<Image> load(const unsigned char* data, std::size_t size, const std::string& name);
    /// Save as PNG. Prints why and returns false on failure.
    bool save(const std::string& path) const;

    /// A copy scaled to width x height. Each new pixel averages the old ones
    /// it covers, so shrinking stays smooth rather than speckled.
    Image resized(int width, int height) const;
};

#endif //MAPLEENGINE_IMAGE_H
