/**
 * Maple Doom: a Doom style shooter built on Maple Engine, drawn by a
 * raycaster in software.
 *
 * Controls:
 *   W A S D, arrow keys     move and strafe, turn with the arrows
 *   Mouse                   turn
 *   Shift                   run
 *   Left mouse, Ctrl        fire
 *   E, Space                open doors, flip the exit switch
 *   1 - 5, mouse wheel      fist, pistol, shotgun, chaingun, rocket launcher
 *   Tab                     automap
 *   M                       music on / off
 *   Esc                     quit
 *
 * Options:
 *   --art <directory>       replace built in art with PNG files found there
 *   --dump-art <directory>  save the built in art as PNG files, to start from
 *   --screenshot <file>     save the opening view as a PNG, without a window
 */
#include "App.h"
#include "Art.h"
#include "DoomScene.h"
#include "Levels.h"
#include "Renderer.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace {

int usage(const char* program)
{
    fprintf(stderr, "usage: %s [--art <directory>] [--dump-art <directory>] [--screenshot <file>]\n", program);
    return 2;
}

/// Draw the start of the first level without opening a window.
int screenshot(const std::string& path, const std::string& artDirectory)
{
    doom::Art art = doom::createArt();
    if (!artDirectory.empty())
        doom::loadArtOverrides(art, artDirectory);

    doom::Game game(doom::campaign());
    // A moment, for the level's name to show.
    game.tick(doom::Input{});

    doom::Renderer renderer(art);
    Image frame(doom::Renderer::WIDTH, doom::Renderer::HEIGHT);
    renderer.render(game, frame, false);
    return frame.save(path) ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    std::string artDirectory;

    for (int i = 1; i < argc; ++i)
    {
        const bool hasValue = i + 1 < argc;
        if (!std::strcmp(argv[i], "--art") && hasValue)
        {
            artDirectory = argv[++i];
        }
        else if (!std::strcmp(argv[i], "--dump-art") && hasValue)
        {
            doom::Art art = doom::createArt();
            const int saved = doom::saveArt(art, argv[++i]);
            printf("Saved %d pictures to %s\n", saved, argv[i]);
            return saved > 0 ? 0 : 1;
        }
        else if (!std::strcmp(argv[i], "--screenshot") && hasValue)
        {
            const std::string path = argv[++i];
            return screenshot(path, artDirectory);
        }
        else
        {
            return usage(argv[0]);
        }
    }

    // Twice the renderer's resolution, so every pixel shows as a square of
    // four.
    App app(doom::Renderer::WIDTH * 2, doom::Renderer::HEIGHT * 2, "Maple Doom", false);

    World::getInstance().setScene([artDirectory](World& game) { createDoomScene(game, artDirectory); });
    app.Launch();
    return 0;
}
