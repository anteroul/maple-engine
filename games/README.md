# Games

Games built on MapleEngine. They are built along with the engine (see the
[main README](../README.md)), and the commands below run from the repository
root.

## Maple Tetris

`games/tetris` is a Tetris clone built on the engine. It follows the modern
guideline: SRS rotation with wall kicks, a 7-bag randomiser, hold, a
three-piece preview, a ghost piece, lock delay and guideline scoring. Cleared
rows break up into Box2D bodies that tumble off the board.

![Maple Tetris](../docs/tetris.png)

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
| M                      | Music on / off              |
| Esc                    | Quit                        |

All of its sound, including a chiptune arrangement of Korobeiniki (the
public-domain folk song behind the Tetris theme), is synthesized at start up.
The music speeds up a little with each level.

The side panel shows the next pieces, the held piece, and the score (white),
lines (cyan) and level (orange). The window title spells them out too.

The rules live in `games/tetris/TetrisGame.*`, with no engine dependencies, so
`tests/tetris_tests.cpp` checks them headless.

## Maple Sandbox

`games/sandbox` is a falling sand playground in the style of Noita. Every
pixel is simulated: sand piles up, water finds its level, oil floats on it,
fire spreads through wood and oil and leaves smoke and ash, gunpowder chains,
water cools lava to stone and boils to steam, and steam that cools rains back
down. Torches and explosions add particle fire, smoke and sparks on top.

![The hut burning down and the ledge giving way under its sand](../docs/sandbox-burning.png)

```
./build/MapleSandbox       # or: nix run .#sandbox
```

| Input                  | Action                                    |
|------------------------|-------------------------------------------|
| Left mouse             | Paint the selected material               |
| Right mouse            | Erase, torches included                   |
| 1 - 9, 0               | Sand, water, stone, wood, oil, gunpowder, lava, fire, smoke, steam |
| Mouse wheel            | Next / previous material                  |
| [ / ], - / =           | Brush size                                |
| T                      | Plant a torch                             |
| E, middle mouse        | Explosion                                 |
| Space, P               | Pause (painting still works)              |
| R                      | Reset the level                           |
| C                      | Clear everything                          |
| B                      | Bloom on / off                            |
| Esc                    | Quit                                      |

The selected material is highlighted in the palette at the top and named in
the window title. The rules live in `games/sandbox/Simulation.*`, with no
engine dependencies, so `tests/sandbox_tests.cpp` checks them headless.

