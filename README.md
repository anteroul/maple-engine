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

## Physics

Simulation is Box2D's, not the engine's. The world carries real gravity, and
the solver owns gravity, contacts, friction and restitution. `World::update`
steps that world exactly once per frame on a fixed 1/60 s timestep, and the
result is copied into each entity's `transform` afterwards.

Entities pick how they are simulated when they are created:

```cpp
// never moves: terrain, walls
new Entity(world, topLeft, bottomRight, BodyType::Static);

// moved by script, pushes dynamic bodies, ignores gravity
new Entity(world, topLeft, bottomRight, BodyType::Kinematic);

// fully simulated
new Entity(world, topLeft, bottomRight, BodyType::Dynamic);
```

Surface properties come from a `PhysicsMaterial` passed alongside, and a
`RigidBody` component gives the entity a mass plus the gameplay-facing calls:
`applyForce`, `applyImpulse`, `getVelocity`, `isGrounded`.

Components must never move a simulated body with `b2Body_SetTransform`, which
teleports it past whatever is in between: write a force or a velocity and let
the solver move it.

### Tests

The tests are headless, so they need no display:

```
cmake --build build
ctest --test-dir build --output-on-failure
```

Configure with `-DMAPLE_BUILD_TESTS=OFF` to skip them.

## Scenes

A game hands the engine a function that builds its scene, and can choose
which part of world space the camera shows:

```cpp
void createMyScene(World& game)
{
    game.setView(-1.f, 17.f, -1.f, 21.f);  // letterboxed, aspect preserved
    game.addEntity(new Entity(...));
}

int main()
{
    App app(540, 660, "My Game", false);
    World::getInstance().setScene(createMyScene);
    app.Launch();
}
```

`World::addEntity()` and `World::destroyEntity()` are safe to call from inside
a component's update: both take effect at the end of the current update.

## Maple Tetris

`games/tetris` is a Tetris clone built on the engine. It follows the modern
guideline: SRS rotation with wall kicks, a 7-bag randomiser, hold, a
three-piece preview, a ghost piece, lock delay and guideline scoring. Cleared
rows break up into Box2D bodies that tumble off the board.

```
./build/MapleTetris        # or: nix run .#tetris
```

| Key                    | Action                      |
|------------------------|-----------------------------|
| Left / Right, A / D    | Move                        |
| Down, S                | Soft drop                   |
| Space                  | Hard drop                   |
| Up, X, W               | Rotate clockwise            |
| Z, Ctrl                | Rotate counter-clockwise    |
| C, Shift               | Hold                        |
| P                      | Pause                       |
| R                      | Restart                     |
| Esc                    | Quit                        |

The side panel shows the next pieces, the held piece, and the score (white),
lines (cyan) and level (orange). The window title spells them out too.

The rules live in `games/tetris/TetrisGame.*`, with no engine dependencies, so
`tests/tetris_tests.cpp` checks them headless.
