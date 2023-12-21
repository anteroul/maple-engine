cmake_minimum_required(VERSION 3.24)
project(MapleEngine)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})

find_package(glfw3 REQUIRED)
include_directories(${GLFW_INCLUDE_DIRS})
link_libraries(${GLFW_LIBRARY_DIRS})

find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})
link_libraries(${FREETYPE_LIBRARY_DIRS})

add_subdirectory(3rd_party/Box2D-cmake)

add_executable(MapleEngine
        src/entrypoint/main.cpp
        src/App.h
        src/App.cpp
        src/Log/msg.h
        src/Scene.cpp
        src/Scene.h
        src/Physics.cpp
        src/Physics.h
        src/ECS/Entity.cpp
        src/ECS/Entity.h
        src/ECS/Component.h
        src/ECS/Components/PlayerInput.cpp
        src/ECS/Components/PlayerInput.h
        src/ECS/Components/BoxRenderer.cpp
        src/ECS/Components/BoxRenderer.h
        src/ECS/Components/SphereRenderer.cpp
        src/ECS/Components/SphereRenderer.h
        src/ECS/Components/MousePosition.cpp
        src/ECS/Components/MousePosition.h
        src/ECS/Components/Gravity.cpp
        src/ECS/Components/Gravity.h
        src/ECS/Components/Shader.cpp
        src/ECS/Components/Shader.h
        src/structs/cursor.h
)

target_link_libraries(MapleEngine PUBLIC ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${FREETYPE_LIBRARIES} ${lib/stb_image.h} -lglfw anteroul::Box2D)