#include "Image.h"
#include "../Log/msg.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace {

/// Take ownership of what stb decoded, or say why it could not.
std::optional<Image> decoded(stbi_uc* data, int width, int height, const std::string& name)
{
    if (!data)
    {
        printf("%s \bWARNING: Could not load image %s: %s\n", WARNING, name.c_str(), stbi_failure_reason());
        return std::nullopt;
    }

    Image image(width, height);
    std::memcpy(image.pixels.data(), data, image.pixels.size() * sizeof(std::uint32_t));
    stbi_image_free(data);
    return image;
}

} // namespace

std::optional<Image> Image::load(const std::string& path)
{
    int width, height, channels;
    // Always ask for four channels, whatever the file holds, so every image
    // comes out the same shape.
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    return decoded(data, width, height, path);
}

std::optional<Image> Image::load(const unsigned char* bytes, std::size_t size, const std::string& name)
{
    int width, height, channels;
    stbi_uc* data = stbi_load_from_memory(bytes, (int)size, &width, &height, &channels, 4);
    return decoded(data, width, height, name);
}

Image Image::resized(int newWidth, int newHeight) const
{
    Image result(newWidth, newHeight);
    if (empty() || newWidth <= 0 || newHeight <= 0)
        return result;

    const float scaleX = (float)width / (float)newWidth;
    const float scaleY = (float)height / (float)newHeight;

    for (int y = 0; y < newHeight; ++y)
        for (int x = 0; x < newWidth; ++x)
        {
            // The rectangle of old pixels this new one covers.
            const float left = (float)x * scaleX, right = left + scaleX;
            const float top = (float)y * scaleY, bottom = top + scaleY;

            // Weighted by how much of each old pixel is covered, and by its
            // alpha, so see through pixels lend no colour to their neighbours.
            float r = 0.f, g = 0.f, b = 0.f, a = 0.f, total = 0.f;
            for (int sy = (int)top; sy < std::min((int)std::ceil(bottom), height); ++sy)
                for (int sx = (int)left; sx < std::min((int)std::ceil(right), width); ++sx)
                {
                    const float coverage = (std::min(right, (float)sx + 1.f) - std::max(left, (float)sx))
                                           * (std::min(bottom, (float)sy + 1.f) - std::max(top, (float)sy));
                    const std::uint32_t pixel = at(sx, sy);
                    const float alpha = (float)alphaOf(pixel) * coverage;
                    r += (float)redOf(pixel) * alpha;
                    g += (float)greenOf(pixel) * alpha;
                    b += (float)blueOf(pixel) * alpha;
                    a += alpha;
                    total += coverage;
                }

            if (a > 0.f)
                result.at(x, y) = rgba((int)std::lround(r / a), (int)std::lround(g / a), (int)std::lround(b / a),
                                       (int)std::lround(a / total));
        }
    return result;
}

bool Image::save(const std::string& path) const
{
    if (empty() || !stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4))
    {
        printf("%s \bWARNING: Could not save image %s\n", WARNING, path.c_str());
        return false;
    }
    return true;
}
