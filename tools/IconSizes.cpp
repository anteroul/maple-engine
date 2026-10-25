// Scales the application icon to the sizes desktops look for, laid out as an
// icon theme: <directory>/<size>x<size>/apps/maple-engine.png.
//
//   MapleIconSizes <source.png> <directory>
#include "Rendering/Image.h"
#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <source.png> <directory>\n", argv[0]);
        return 2;
    }

    const std::optional<Image> source = Image::load(argv[1]);
    if (!source)
        return 1;

    for (int size: {16, 24, 32, 48, 64, 128, 256, 512})
    {
        const std::filesystem::path directory = std::filesystem::path(argv[2])
                                                / (std::to_string(size) + "x" + std::to_string(size)) / "apps";
        std::filesystem::create_directories(directory);
        if (!source->resized(size, size).save((directory / "maple-engine.png").string()))
            return 1;
    }
    return 0;
}
