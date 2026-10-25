# MapleEngine
2D Game Engine with ECS. The engine uses Box2D for physics and OpenGL for rendering.

![A gunpowder stash going off in Maple Sandbox](docs/sandbox-explosion.png)

## Installation:
Install required dependencies:
- Ubuntu/Debian: `apt install build-essential cmake libgl-dev libglm-dev libglew-dev libglfw3-dev`
- Arch/Manjaro: `pacman -S base-devel cmake glut glm glfw glew`

Audio uses [miniaudio](https://miniaud.io). If it is installed (so that
`pkg-config miniaudio` finds it) that copy is used; otherwise CMake downloads
miniaudio 0.11.25 and builds it with the engine.

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

Every window shows the Maple Engine leaf, `docs/maple-engine.png`, built into
the executable. Wayland gives windows no way to set their own icon, so there
the desktop finds it through the window's application id, its title in lower
case with dashes ("Maple Tetris" is `maple-tetris`), and the matching `.desktop`
file in `packaging/`. `nix profile install` puts those and the icon in place.

## Bloom and glow

Bloom is off by default. Turned on, the scene is drawn into a floating point
buffer, and whatever is brighter than the threshold spills light around it:

```cpp
BloomSettings bloom;
bloom.enabled = true;
bloom.threshold = 1.f;   // only colours brighter than white bloom
bloom.intensity = 1.f;
game.setBloom(bloom);

renderer->setColour({4.f, 1.5f, 0.5f});  // brighter than white: blooms orange
```

A `Glow` component makes one entity shine however bright it is drawn, in its
own colour or in one of its own:

```cpp
#include "ECS/Components/Glow.h"

entity->addComponent(new Glow(*entity, 1.5f));                      // own colour
entity->addComponent(new Glow(*entity, glm::vec3{0.4f, 0.8f, 1.f}, 3.f));
```

Bloom needs OpenGL 3.0. Without it the engine prints a warning and draws the
scene as before.

## Fire and smoke

`Effects/ParticleEffects.h` has ready-made particle effects: `fire`, `smoke`,
`embers`, `sparks` and `explosion`. Each takes the width of the effect in
world units and scales the rest to match. Fire, embers and sparks are brighter
than white, so they glow with bloom on.

Attach one to an entity with a `ParticleEmitter`:

```cpp
#include "ECS/Components/ParticleEmitter.h"
#include "Effects/ParticleEffects.h"

// Components draw in the order they are added: smoke first, so the flame
// shows through it.
torch->addComponent(new ParticleEmitter(*torch, effects::smoke(0.4f), {0.f, 0.3f}));
torch->addComponent(new ParticleEmitter(*torch, effects::fire(0.3f)));
```

or drive a `ParticleSystem` yourself, for one-off bursts:

```cpp
ParticleSystem sparks(effects::sparks(0.5f));
sparks.burst({x, y}, 40);   // then sparks.update(dt) and sparks.render() each frame
```

Every number in a `ParticleEffect` can be changed, so a preset is only a
starting point: green fire is `fire()` with different colours.

![An oil slick burning on a pool of water](docs/sandbox-oil-fire.png)

## Compute shaders

Where OpenGL 4.3 is available, particles run on the GPU: the CPU only works
out where new ones start, and a compute shader (`src/Shaders/particles.comp`)
moves them all and builds the quads they are drawn with. The sandbox colours
its pixels the same way (`games/sandbox/shaders/cells.comp`). Elsewhere, macOS
included, the engine prints a warning and does the same work on the CPU, and
`MAPLE_NO_COMPUTE=1` forces that, to compare the two.

Shader files are embedded into the executable at build time, so a game never
has to find them at run time: `maple_embed_shader(<target> <file> <NAME>)` in
`CMakeLists.txt` turns a file into `#include "Shaders/<file>.h"`, which
defines `NAME` as its source.

## Audio

`Audio::getInstance()` plays sound effects fire-and-forget, plus one looping
music track. Sounds can come from files (WAV, FLAC, MP3) or be generated in
code with the chiptune synthesizer in `src/Audio/Synth.h`, so a game needs no
audio files at all:

```cpp
#include "Audio/Audio.h"
#include "Audio/Synth.h"

// A short rising blip.
auto jump = synth::share(synth::render({{synth::Waveform::Square, 400.f, 800.f, 0.1f}}));
Audio::getInstance().play(jump);

Audio::getInstance().play("sounds/explosion.wav", 0.8f);
Audio::getInstance().playMusic("music/theme.mp3", 0.5f);
```

If no audio device can be opened, the engine prints a warning and every call
does nothing, so games never need to check first.

## Images

`Rendering/Image.h` holds a picture in memory as 8 bit RGBA, for textures and
sprites worked on by the CPU. It loads PNG, JPEG, BMP, TGA and GIF files and
saves PNG, using [stb_image](https://github.com/nothings/stb). Like miniaudio,
an installed copy of stb is used if `pkg-config stb` finds one, and otherwise
CMake downloads it.

```cpp
#include "Rendering/Image.h"

if (std::optional<Image> brick = Image::load("textures/brick.png"))
{
    // One pixel, packed as rgba() packs it: redOf(), alphaOf()... unpack it.
    const std::uint32_t texel = brick->at(10, 20);
}

Image frame(320, 200, rgba(0, 0, 0));
frame.save("frame.png");
```

`PixelCanvas::upload(image)` puts a whole image on screen in one go: the way to
show frames drawn by a software renderer.

## Games

`games/` holds Maple Tetris and Maple Sandbox. See
[games/README.md](games/README.md) for how to run and play them.
