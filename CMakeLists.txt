cmake_minimum_required(VERSION 3.24)
project(MapleEngine)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})

find_package(glfw3 REQUIRED)
include_directories(${GLFW_INCLUDE_DIRS})
link_libraries(${GLFW_LIBRARY_DIRS})

add_executable(MapleEngine
        src/entrypoint/main.cpp
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
)

target_link_libraries(MapleEngine PUBLIC ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${lib/stb_image.h} -lglfw -lbox2d)