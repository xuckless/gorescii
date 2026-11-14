# TextGme

A 2.5D first-person shooter that runs in your terminal. The world is drawn
by a software sector renderer, then turned into coloured ASCII by a CPU
take on the ASCII shader from
[AcerolaFX](https://github.com/GarrettGunnell/AcerolaFX).

## Build and run

Needs a C++20 compiler and CMake 3.20+ (macOS or Linux). No other dependencies.

```sh
cmake -S . -B build
cmake --build build -j
./build/textgme
ctest --test-dir build      # unit tests
```

Sound effects are synthesised at startup and played through `afplay`
(macOS) or `paplay`/`aplay` (Linux) when one is installed.

Use a truecolor terminal. Ghostty, kitty, WezTerm, foot and recent iTerm2
also report key releases (kitty keyboard protocol), which makes movement
feel right; elsewhere the game falls back to key repeat. Bigger windows and
smaller fonts mean more detail.

| Flag | |
|---|---|
| `--map FILE` | play one map instead of the campaign (`assets/maps/campaign.txt`; format in `assets/maps/README.md`) |
| `--cell-width N` | pixels rendered per cell column (default 4; lower is faster) |
| `--mouse-sens X` / `--no-mouse` | mouse look sensitivity, or turn it off |
| `--no-sound` | no sound effects |
| `--style clean\|lines\|off` | edge look: geometry only (default), texture detail in line glyphs too, or none |
| `--snapshot 120x40 [--at X,Y,DEG]` | print one frame as plain text and exit |

## Controls

| | |
|---|---|
| W A S D | move / strafe (always run; hold Shift to walk) |
| ← → or J L, mouse | turn; park the pointer at a window edge to keep turning |
| Space, Ctrl, left click | fire |
| E | open doors, call lifts |
| 1 2, Q, Tab, wheel, right click | switch weapon |
| Esc | pause (X quits from the pause screen) |
| C / V / B | colour mode / edge style (clean, lines, off) / background fill |
| M | mute / unmute |
| Enter | restart after dying or finishing |
| Ctrl-C | quit |

## How it works

```
map (convex sectors) -> portal renderer -> colour + depth + surface-id framebuffer
                     -> sprites, weapon, flashes
                     -> ASCII pass -> cell grid -> HUD -> diffed ANSI output
```

- **Renderer** (`src/render`): starts in the camera's sector and recurses
  through portals, keeping per-column clip bounds. It draws textured walls,
  upper and lower steps, per-pixel floors and ceilings, a sky panorama and
  depth-tested billboard sprites. Textures and sprite art are generated in code.
- **ASCII pass** (`src/gfx/ascii.cpp`): each terminal cell covers a 4×8 pixel
  tile.
  - Tile luminance picks a fill glyph from ` .:coPO?@#`.
  - Difference-of-Gaussians plus Sobel finds edges and buckets their angle
    into `| _ / \`. Jumps in depth and in surface id (the stand-in for
    AcerolaFX's normals) add edges too.
  - Each cell votes, and if enough pixels agree the line glyph wins over the fill.
  - Cell colour comes from the image, normalised so dark areas aren't darkened twice.
- **Presenter** (`src/gfx/present.cpp`): sends only the cells that changed, as
  truecolor escapes wrapped in synchronized updates.
- **Game** (`src/game`): circle-vs-segment collision with steps and gravity,
  line-of-sight traces through portals, grunts with hitscan, brutes with
  fireballs, pickups, slime and the exit.
