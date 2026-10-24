#ifndef MAPLEENGINE_PIXELCANVAS_H
#define MAPLEENGINE_PIXELCANVAS_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

/// A grid of pixels, written from the CPU and drawn as one textured rectangle
/// in world space. For pixel simulations, minimaps, procedural images.
///
/// Pixels are floating point RGBA: colours above 1 glow with bloom on, and
/// alpha blends a pixel over what is behind it, so alpha 0 leaves the
/// background showing.
class PixelCanvas {
public:
    PixelCanvas(int width, int height);
    ~PixelCanvas();
    PixelCanvas(const PixelCanvas&) = delete;
    PixelCanvas& operator=(const PixelCanvas&) = delete;

    int getWidth() const { return m_Width; }
    int getHeight() const { return m_Height; }

    /// (0, 0) is the bottom left pixel.
    void setPixel(int x, int y, const glm::vec4& colour)
    {
        m_Pixels[(std::size_t)y * m_Width + x] = colour;
        m_Dirty = true;
    }
    void fill(const glm::vec4& colour);

    /// Draw over [bottomLeft, bottomLeft + size] with the current
    /// projection, uploading the pixels first if they changed.
    void draw(glm::vec2 bottomLeft, glm::vec2 size) const;

    /// Bind the canvas to image unit `unit` as a write-only rgba16f image,
    /// for a compute shader to fill in instead of setPixel. What it writes
    /// replaces the pixels set from the CPU until the next setPixel or fill.
    void bindImage(GLuint unit);

private:
    /// Creates the texture on first use, when there is sure to be a GL context.
    void createTexture() const;

    int m_Width;
    int m_Height;
    std::vector<glm::vec4> m_Pixels;
    mutable GLuint m_Texture = 0;
    mutable bool m_Dirty = true;
};

#endif //MAPLEENGINE_PIXELCANVAS_H
