cmake_minimum_required(VERSION 3.24)
project(MapleEngine)

set(CMAKE_CXX_STANDARD 23)

option(MAPLE_BUILD_TESTS "Build the headless tests" ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

# miniaudio. Prefer an installed copy: distributions that package it (nixpkgs
# does) link its backends directly, which avoids loading PulseAudio or ALSA by
# name at runtime, something that fails on NixOS. Otherwise build it from
# source, letting it find the backends at runtime.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MINIAUDIO IMPORTED_TARGET miniaudio)
endif()

if(MINIAUDIO_FOUND)
    add_library(maple_miniaudio INTERFACE)
    # Installed headers live in include/miniaudio/, so add that too and let
    # the source say #include <miniaudio.h> either way.
    target_include_directories(maple_miniaudio INTERFACE ${MINIAUDIO_INCLUDEDIR}/miniaudio)
    target_link_libraries(maple_miniaudio INTERFACE PkgConfig::MINIAUDIO)
else()
    message(STATUS "miniaudio not installed, building it from source")
    include(FetchContent)
    FetchContent_Declare(miniaudio
            URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz
            # Only the sources are wanted, not miniaudio's own build.
            SOURCE_SUBDIR do-not-build)
    FetchContent_MakeAvailable(miniaudio)

    add_library(maple_miniaudio STATIC ${miniaudio_SOURCE_DIR}/miniaudio.c)
    target_include_directories(maple_miniaudio PUBLIC ${miniaudio_SOURCE_DIR})
    target_link_libraries(maple_miniaudio PUBLIC ${CMAKE_DL_LIBS} pthread m)
endif()

# The engine, shared by the demo and the games built on it.
add_library(MapleEngineCore STATIC
        src/App.cpp
        src/World.cpp
        src/Physics.cpp
        src/Audio/Audio.cpp
        src/Audio/Synth.cpp
        src/ECS/Entity.cpp
        src/ECS/Components/BoxRenderer.cpp
        src/ECS/Components/SphereRenderer.cpp
        src/ECS/Components/MouseFollow.cpp
        src/ECS/Components/RigidBody.cpp
        src/ECS/Components/UserInput.cpp
        src/ECS/Components/Shader.cpp
        src/ECS/Rendering/MeshGPU.cpp
        src/ECS/Geometry/SphereGeometry.cpp
        src/ECS/Geometry/BoxGeometry.cpp
)

# Games include engine headers as "World.h", "ECS/...", "Scripts/...".
target_include_directories(MapleEngineCore PUBLIC
        src ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})
target_link_libraries(MapleEngineCore PUBLIC ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} -lGLX -lglfw -lbox2d maple_miniaudio)

# The engine's sandbox scene.
add_executable(MapleEngine
        src/entrypoint/main.cpp
        src/Scenes/DemoScene.cpp
)
target_link_libraries(MapleEngine PRIVATE MapleEngineCore)

# The rules have no engine dependencies, so the tests can link them alone.
add_library(MapleTetrisRules STATIC games/tetris/TetrisGame.cpp)
target_include_directories(MapleTetrisRules PUBLIC games/tetris)

add_executable(MapleTetris
        games/tetris/main.cpp
        games/tetris/TetrisController.cpp
        games/tetris/TetrisScene.cpp
        games/tetris/TetrisAudio.cpp
)
target_link_libraries(MapleTetris PRIVATE MapleEngineCore MapleTetrisRules)

if(MAPLE_BUILD_TESTS)
    # The tests never open a window, so they run without a display.
    enable_testing()

    add_executable(MapleTetrisTests tests/tetris_tests.cpp)
    target_link_libraries(MapleTetrisTests PRIVATE MapleTetrisRules)
    add_test(NAME tetris COMMAND MapleTetrisTests)
endif()
