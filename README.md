# MapleEngine
2D Game Engine with ECS. The engine uses Box2D for physics and OpenGL for rendering.

## Installation:
Install required dependencies:
- Ubuntu/Debian: `apt install build-essential cmake libgl-dev libglm-dev libglew-dev libglfw3-dev`
- Arch/Manjaro: `pacman -S base-devel cmake glut glm glfw glew`

Clone the repository:

```
git clone https://github.com/anteroul/MapleEngine.git
cd MapleEngine
```

The engine uses the Box2D 3.x C API (`b2WorldId`, `b2CreateWorld`, ...), so the
2.4 packages most distributions ship will not work. Build and install Box2D 3
from the bundled submodule:

```
git submodule update --init --recursive
cmake -S 3rd_party/box2d -B 3rd_party/box2d/build -DCMAKE_BUILD_TYPE=Release \
      -DBOX2D_SAMPLES=OFF -DBOX2D_UNIT_TESTS=OFF -DBOX2D_BENCHMARKS=OFF
cmake --build 3rd_party/box2d/build
sudo cmake --install 3rd_party/box2d/build
```

Then build the engine:

```
cmake -S . -B build
cmake --build build
./build/MapleEngine
```
