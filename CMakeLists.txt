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
        src/App.h
        src/App.cpp
        src/Log/msg.h
        src/World.cpp
        src/World.h
        src/Physics.cpp
        src/Physics.h
        src/ECS/Entity.cpp
        src/ECS/Entity.h
        src/ECS/Component.h
        src/ECS/Components/BehaviourScript.h
        src/ECS/Components/BoxRenderer.cpp
        src/ECS/Components/BoxRenderer.h
        src/ECS/Components/SphereRenderer.cpp
        src/ECS/Components/SphereRenderer.h
        src/ECS/Components/MouseFollow.cpp
        src/ECS/Components/MouseFollow.h
        src/ECS/Components/RigidBody.cpp
        src/ECS/Components/RigidBody.h
        src/ECS/Components/UserInput.cpp
        src/ECS/Components/UserInput.h
        src/ECS/Components/Shader.h
        src/ECS/Components/Shader.cpp
        src/ECS/Components/Shader.cpp
        src/Scripts/MouseFollow.h
)

target_link_libraries(MapleEngine PUBLIC ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${lib/stb_image.h} -lglfw -lbox2d)