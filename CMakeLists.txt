cmake_minimum_required(VERSION 3.24)
project(MapleEngine)

set(CMAKE_CXX_STANDARD 23)
# Write compile_commands.json into the build directory, for clangd and other
# tools that need each file's compiler flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# stb_image and stb_image_write, for loading and saving images. Like
# miniaudio, prefer an installed copy (nixpkgs has one) and otherwise fetch
# the headers.
if(PkgConfig_FOUND)
    pkg_check_modules(STB IMPORTED_TARGET stb)
endif()

add_library(maple_stb INTERFACE)
if(STB_FOUND)
    target_link_libraries(maple_stb INTERFACE PkgConfig::STB)
else()
    message(STATUS "stb not installed, fetching it")
    include(FetchContent)
    FetchContent_Declare(stb
            URL https://github.com/nothings/stb/archive/003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5.tar.gz
            SOURCE_SUBDIR do-not-build)
    FetchContent_MakeAvailable(stb)
    target_include_directories(maple_stb INTERFACE ${stb_SOURCE_DIR})
endif()

# Images on the CPU. No GL, so headless tools and tests can use it too.
add_library(MapleImage STATIC src/Rendering/Image.cpp)
target_include_directories(MapleImage PUBLIC src)
target_link_libraries(MapleImage PRIVATE maple_stb)

# Embeds a shader file into the target as a header, generated/Shaders/<file>.h,
# that defines the string NAME. Editing the shader rebuilds what includes it.
function(maple_embed_shader target source name)
    get_filename_component(file "${source}" NAME)
    set(header "${CMAKE_BINARY_DIR}/generated/Shaders/${file}.h")
    add_custom_command(
            OUTPUT "${header}"
            COMMAND ${CMAKE_COMMAND} -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${source}
                    -DOUTPUT=${header} -DNAME=${name} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShader.cmake
            DEPENDS "${source}" cmake/EmbedShader.cmake
            COMMENT "Embedding ${source}"
            VERBATIM)
    target_sources(${target} PRIVATE "${header}")
    target_include_directories(${target} PRIVATE "${CMAKE_BINARY_DIR}/generated")
endfunction()

# Embeds any file into the target as a header, generated/Embedded/<file>.h,
# that defines the byte array NAME and its length NAME_SIZE.
function(maple_embed_file target source name)
    get_filename_component(file "${source}" NAME)
    set(header "${CMAKE_BINARY_DIR}/generated/Embedded/${file}.h")
    add_custom_command(
            OUTPUT "${header}"
            COMMAND ${CMAKE_COMMAND} -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${source}
                    -DOUTPUT=${header} -DNAME=${name} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake
            DEPENDS "${source}" cmake/EmbedBinary.cmake
            COMMENT "Embedding ${source}"
            VERBATIM)
    target_sources(${target} PRIVATE "${header}")
    target_include_directories(${target} PRIVATE "${CMAKE_BINARY_DIR}/generated")
endfunction()

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
        src/Rendering/PostProcess.cpp
        src/Rendering/ShaderProgram.cpp
        src/Rendering/PixelCanvas.cpp
        src/Effects/ParticleSystem.cpp
        src/ECS/Geometry/SphereGeometry.cpp
        src/ECS/Geometry/BoxGeometry.cpp
)

# Games include engine headers as "World.h", "ECS/...", "Scripts/...".
target_include_directories(MapleEngineCore PUBLIC
        src ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})
target_link_libraries(MapleEngineCore PUBLIC ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} -lGLX -lglfw -lbox2d maple_miniaudio MapleImage)
maple_embed_shader(MapleEngineCore src/Shaders/particles.comp PARTICLES_COMP)
# Every game's window icon.
maple_embed_file(MapleEngineCore docs/maple-engine.png APP_ICON_PNG)

# The icon in the sizes desktops look for, for installing with the .desktop
# files in packaging/: build/icons/hicolor/<size>x<size>/apps/maple-engine.png.
add_executable(MapleIconSizes tools/IconSizes.cpp)
target_link_libraries(MapleIconSizes PRIVATE MapleImage)
add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/icons/hicolor/256x256/apps/maple-engine.png"
        COMMAND MapleIconSizes ${CMAKE_CURRENT_SOURCE_DIR}/docs/maple-engine.png ${CMAKE_BINARY_DIR}/icons/hicolor
        DEPENDS MapleIconSizes docs/maple-engine.png
        COMMENT "Scaling the application icon"
        VERBATIM)
add_custom_target(MapleIcons ALL DEPENDS "${CMAKE_BINARY_DIR}/icons/hicolor/256x256/apps/maple-engine.png")

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

# Like the Tetris rules, the falling sand simulation stands alone.
add_library(MapleSandboxRules STATIC
        games/sandbox/Simulation.cpp
        games/sandbox/Level.cpp
)
target_include_directories(MapleSandboxRules PUBLIC games/sandbox)
# Hundreds of thousands of cell updates a second: worth optimising even in
# a debug build.
target_compile_options(MapleSandboxRules PRIVATE $<$<NOT:$<CONFIG:Debug>>:-O2>)

add_executable(MapleSandbox
        games/sandbox/main.cpp
        games/sandbox/SandboxController.cpp
        games/sandbox/SandboxScene.cpp
        games/sandbox/CellColourer.cpp
)
target_link_libraries(MapleSandbox PRIVATE MapleEngineCore MapleSandboxRules)
maple_embed_shader(MapleSandbox games/sandbox/shaders/cells.comp CELLS_COMP)

# Maple Doom. It is not in git, so it is only built where its sources are on
# disk. The rules stand alone, like the other games'. The software renderer
# and its art need images but no GL, so they can draw frames headless too.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/games/doom/main.cpp)
    add_library(MapleDoomRules STATIC
            games/doom/Map.cpp
            games/doom/Raycast.cpp
            games/doom/Game.cpp
            games/doom/Levels.cpp
    )
    target_include_directories(MapleDoomRules PUBLIC games/doom)

    add_library(MapleDoomView STATIC
            games/doom/Art.cpp
            games/doom/Font.cpp
            games/doom/Renderer.cpp
    )
    target_link_libraries(MapleDoomView PUBLIC MapleDoomRules MapleImage)

    # The renderer draws every pixel of every frame on the CPU, and the rules
    # run the monsters: both are worth optimising even in a debug build.
    target_compile_options(MapleDoomRules PRIVATE $<$<NOT:$<CONFIG:Debug>>:-O2>)
    target_compile_options(MapleDoomView PRIVATE $<$<NOT:$<CONFIG:Debug>>:-O2>)

    add_executable(MapleDoom
            games/doom/main.cpp
            games/doom/DoomController.cpp
            games/doom/DoomScene.cpp
            games/doom/DoomAudio.cpp
    )
    target_link_libraries(MapleDoom PRIVATE MapleEngineCore MapleDoomView)
endif()

if(MAPLE_BUILD_TESTS)
    # The tests never open a window, so they run without a display.
    enable_testing()

    add_executable(MapleTetrisTests tests/tetris_tests.cpp)
    target_link_libraries(MapleTetrisTests PRIVATE MapleTetrisRules)
    add_test(NAME tetris COMMAND MapleTetrisTests)

    add_executable(MapleSandboxTests tests/sandbox_tests.cpp)
    target_link_libraries(MapleSandboxTests PRIVATE MapleSandboxRules)
    add_test(NAME sandbox COMMAND MapleSandboxTests)

    if(TARGET MapleDoomRules AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/doom_tests.cpp)
        add_executable(MapleDoomTests tests/doom_tests.cpp)
        target_link_libraries(MapleDoomTests PRIVATE MapleDoomRules)
        add_test(NAME doom COMMAND MapleDoomTests)
    endif()
endif()
