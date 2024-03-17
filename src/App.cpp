#include "App.h"
#include "Log/msg.h"

App::App(unsigned int width, unsigned int height, char* windowTitle, bool fullscreen)
{
    if (!glfwInit())
        ThrowError(EXIT_FAILURE, "Unable to initialize GLFW.");
    else
        printf("%s \bSUCCESS: GLFW initialized successfully.\n", SUCCESS);

    glfwSetErrorCallback(ThrowError);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow((int)width, (int)height, windowTitle, nullptr, nullptr);

    if (!window)
        ThrowError(EXIT_FAILURE, "Failed to create OpenGL context window.");

    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, HandleMouseMotion);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    if (fullscreen)
        glfwMaximizeWindow(window);

    if (!glfwInit())
        ThrowError(EXIT_FAILURE, "Unable to initialize GLEW.");
    else
        printf("%s \bSUCCESS: GLEW initialized successfully.\n", SUCCESS);
}

App::~App()
{
    glfwSetWindowShouldClose(window, true);
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

    Scene& game = Scene::getInstance();
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
void App::RunApplication(Scene& game)
{
    game.update(window, (float)deltaTime);
    HandleMouseMotion(window, 0, 0);
    game.render(window);
    glfwPollEvents();
}

bool App::ApplicationShouldClose()
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
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

void App::ThrowError(int error, const char *description)
{
    fprintf(stderr, "ERROR %d: \t %s\n", error, description);
    exit(error);
}
