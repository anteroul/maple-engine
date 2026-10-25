#include "App.h"
#include "Log/msg.h"
#include "Audio/Audio.h"
#include "Rendering/Image.h"
#include "Embedded/maple-engine.png.h"
#include <cctype>
#include <string>
#include <vector>

namespace {

/// Which name the desktop knows this program by: the title in lower case,
/// spaces as dashes, "Maple Doom" as maple-doom. On Wayland the desktop
/// finds the program's icon through this, in the .desktop file of that
/// name, since a window cannot set its own icon there.
std::string applicationId(const char* title)
{
    std::string id;
    for (const char* c = title; *c; ++c)
        id += std::isalnum((unsigned char)*c) ? (char)std::tolower((unsigned char)*c) : '-';
    return id;
}

/// Errors from setting the icon are not worth quitting over.
void WarnAboutIcon(int, const char* description)
{
    printf("%s \bWARNING: Could not set the window icon: %s\n", WARNING, description);
}

} // namespace

App::App(unsigned int width, unsigned int height, const char* windowTitle, bool fullscreen)
{
    if (!glfwInit())
        ThrowError(EXIT_FAILURE, "Unable to initialize GLFW.");
    else
        printf("%s \bSUCCESS: GLFW initialized successfully.\n", SUCCESS);

    glfwSetErrorCallback(ThrowError);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    const std::string id = applicationId(windowTitle);
#ifdef GLFW_WAYLAND_APP_ID
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, id.c_str());
#endif
    glfwWindowHintString(GLFW_X11_CLASS_NAME, id.c_str());
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, id.c_str());

    window = glfwCreateWindow((int)width, (int)height, windowTitle, nullptr, nullptr);

    if (!window)
        ThrowError(EXIT_FAILURE, "Failed to create OpenGL context window.");

    glfwMakeContextCurrent(window);
    SetIcon();
    glfwSetCursorPosCallback(window, HandleMouseMotion);
    glfwSetScrollCallback(window, HandleScroll);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    // Input is polled once a frame. Sticky keys and buttons latch a press
    // until the next poll, so a tap shorter than a frame still registers.
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
    glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);

    if (fullscreen)
        glfwMaximizeWindow(window);

    if (glewInit() != GLEW_OK)
        ThrowError(EXIT_FAILURE, "Unable to initialize GLEW.");
    else
        printf("%s \bSUCCESS: GLEW initialized successfully.\n", SUCCESS);
}

App::~App()
{
    glfwSetWindowShouldClose(window, true);
    // Stop the audio thread before the program starts tearing down statics.
    Audio::shutdown();
    World::getInstance().releaseRendering();
    glfwTerminate();
    printf("%s \bSUCCESS: Application closed successfully.\n", SUCCESS);
}

void App::Launch()
{
    int width, height;
    double frameCount = 0.0;
    double totalTime = 0.0;
    double FPS;

    glfwGetWindowSize(window, &width, &height);
    printf("%s \bINFO: Application is now running!\n", INFO);

    World& game = World::getInstance();
    game.initialize();

    while (!ApplicationShouldClose())
    {
        double currentTime = glfwGetTime();
        deltaTime = currentTime - newTime;
        newTime = currentTime;

        RunApplication(game);

        frameCount++;
        totalTime += deltaTime;

        if (totalTime >= 1.0)
        {
            FPS = frameCount / totalTime;
            frameCount = 0.0;
            totalTime = 0.0;
            printf("%s FPS: %.2f\n", INFO, FPS);
        }
    }
}

///  Main game loop
void App::RunApplication(World& game)
{
    game.update(window, (float)deltaTime);
    HandleMouseMotion(window, 0, 0);
    game.render(window);
    glfwPollEvents();
}

bool App::ApplicationShouldClose()
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(window))
    {
        printf("%s \bINFO: Closing application.\n", WARNING);
        return true;
    }
    return false;
}

void App::HandleMouseMotion(GLFWwindow *window, double xPos, double yPos)
{
    int width, height;
    glfwGetCursorPos(window, &xPos, &yPos);
    glfwGetWindowSize(window, &width, &height);
}

/// Scrolling only arrives as events, so it is collected for components to
/// read with World::getScroll().
void App::HandleScroll(GLFWwindow*, double xOffset, double yOffset)
{
    World::getInstance().addScroll(xOffset, yOffset);
}

void App::SetIcon()
{
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
    // Wayland has no way for a window to set its own icon: the desktop
    // takes it from the .desktop file matching the application id instead.
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
        return;
#endif

    const std::optional<Image> source = Image::load(APP_ICON_PNG, APP_ICON_PNG_SIZE, "the application icon");
    if (!source)
        return;

    // A few sizes, so the window manager can pick a sharp one for the title
    // bar and another for the task switcher.
    std::vector<Image> sizes;
    for (int size: {16, 24, 32, 48, 64, 128, 256})
        sizes.push_back(source->resized(size, size));

    std::vector<GLFWimage> images;
    for (Image& image: sizes)
        images.push_back({image.width, image.height, reinterpret_cast<unsigned char*>(image.pixels.data())});

    glfwSetErrorCallback(WarnAboutIcon);
    glfwSetWindowIcon(window, (int)images.size(), images.data());
    glfwSetErrorCallback(ThrowError);
}

void App::ThrowError(int error, const char *description)
{
    fprintf(stderr, "%s \bERROR %d: %s\n", ERROR, error, description);
    exit(error);
}