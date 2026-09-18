cmake_minimum_required(VERSION 3.24)
project(MapleEngine)

set(CMAKE_CXX_STANDARD 23)

option(MAPLE_BUILD_TESTS "Build the headless physics tests" ON)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})

find_package(glfw3 REQUIRED)
include_directories(${GLFW_INCLUDE_DIRS})
link_libraries(${GLFW_LIBRARY_DIRS})

set(MAPLE_ENGINE_SOURCES
        src/App.cpp
        src/World.cpp
        src/Physics.cpp
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

set(MAPLE_ENGINE_LIBRARIES
        ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} -lGLX -lglfw -lbox2d)

add_executable(MapleEngine
        src/entrypoint/main.cpp
        ${MAPLE_ENGINE_SOURCES}
)

target_link_libraries(MapleEngine PUBLIC ${MAPLE_ENGINE_LIBRARIES})

if(MAPLE_BUILD_TESTS)
    # The physics tests never open a window, so they run without a display.
    enable_testing()

    add_executable(MapleEnginePhysicsTests
            tests/physics_tests.cpp
            ${MAPLE_ENGINE_SOURCES}
    )

    target_link_libraries(MapleEnginePhysicsTests PUBLIC ${MAPLE_ENGINE_LIBRARIES})

    add_test(NAME physics COMMAND MapleEnginePhysicsTests)
endif()
