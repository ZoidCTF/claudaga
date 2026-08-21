# Claudaga

A Galaga clone in C99 on SDL2, built with MSVC.

Every pixel it draws is generated. There are no image files: the artwork is
convex polygons, the text is a stroke font drawn as thick quads, and the
explosions, the tractor beam and the starfield are built per frame. Flight paths
are Catmull-Rom splines resampled by arc length, so an enemy's heading falls out
of the curve it is flying.

The wave flies a scripted entry into formation and then attacks. Enemies dive,
shoot back, leave the bottom of the screen and come round again from the top. A
Boss Galaga will drop out of formation and try to take your fighter with a
tractor beam; shoot it before it gets home and you fly the rescued fighter as a
second hull. Every fourth stage from the third is a challenging stage. One or
two players, taking turns.

Sound and music are other people's work and are all public domain — see
[assets/audio/CREDITS.txt](assets/audio/CREDITS.txt). Nothing from Galaga itself
is used, in art or in audio.

## Build

The repository does not carry its dependencies. Fetch them once after cloning:

```bash
tools\fetch_deps.bat
```

That pulls SDL2 2.32.10 and SDL2_mixer 2.8.2 into `third_party/`. Both are
pinned, since they ship prebuilt `.lib` and `.dll` files and an unplanned
upgrade would change what actually links. Re-running it skips whatever is
already there.

```bash
build.bat
```

Finds Visual Studio through `vswhere`, enters the x64 MSVC environment, compiles
everything in `src/`, and stages both DLLs and the `assets/` folder next to the
executable. Pass `release` for an optimised, windowed build; the default is a
debug build on the console subsystem, with the shape browser, the pose check and
the debug keys compiled in.

Output is teed to `build/build.log` and the last line reports a warning count,
because warnings scroll off the top and a build that says only `OK` reads as
clean when it is not. The build is `/W4` and warning-free.

```bash
build\claudaga.exe
```

Runs from anywhere: assets are found relative to the executable through
`SDL_GetBasePath`, not the working directory. The high score and settings are
written to `SDL_GetPrefPath`'s directory.

## Package

```bash
tools\package.bat 1.0
```

Wipes `build/`, makes a release from scratch, and writes `dist/claudaga-1.0.zip`
— around 2.5 MB holding the executable, the two SDL DLLs, `assets/`, and a short
README for whoever opens it. The ZIP does not require installation.

The GitHub Actions workflows build the Windows x64 application on every change.
A tag such as `v1.0.0` also builds an Inno Setup installer and publishes both
`claudaga-1.0.0.zip` and `claudaga-1.0.0-setup.exe` in a GitHub release. Run the
`Windows release` workflow manually to test both packages without publishing a
release. The project does not sign these files, so Windows can identify them as
files from an unknown publisher.

## Controls

| | |
| --- | --- |
| Keyboard | arrows to fly, space to fire |
| Controller | stick or d-pad to fly, A to fire, Start to pause |
| `P` | pause |
| `F11` / `Alt+Enter` | full screen |
| `Escape` | back to the menu |

Debug builds add `Tab` for the shape browser and pose check, `F2` for the path
overlay, `F3` to stop attacks, and `R` to restart. A release build has none of
them, and the title screen does not offer them.

## Layout

```
src/
  common.h      screen geometry, Vec2, heading constants
  gfx.*         window, renderer, fullscreen, screenshots
  shape.*       vector artwork: the renderer and the transform
  shapes.c      the artwork itself, as polygons
  font.*        stroke font: glyphs as line segments, drawn with thickness
  path.*        Catmull-Rom splines resampled by arc length
  formation.*   slots, entries, attack curves, the wave's state machine
  fx.*          explosions and score popups
  game.*        the fighter, shots, starfield, collision, scoring, stages
  audio.*       SDL_mixer: effects, music, and the tools that measure them
  input.*       keyboard and controllers, merged into three booleans
  settings.*    volumes and fullscreen, persisted
  main.c        the loop, the views, and the two-player session
assets/audio/   sound and music, all CC0
third_party/    SDL2 and SDL2_mixer - fetched, not committed
tools/          dependency, package, and installer definitions
```

The picture is 224x288 — the arcade's raster turned upright for a vertical
cabinet. `SDL_RenderSetLogicalSize` keeps paths, speeds and collision radii in
those units whatever the window size.

## Development flags

The game takes a number of flags for checking it without playing it. Most print
numbers rather than opening a window, and they are how the artwork, the wave and
the mix actually get verified.

`--shot out.bmp` renders one frame and exits, `--at N` fast-forwards first, and
`--title` / `--scene` / `--shapes` / `--pose` / `--options` / `--paused` /
`--demo` choose what it renders. `--stats N` runs the wave headless and reports
what the stage's difficulty works out to and how it behaved under it; `--trace`
logs stage handovers, turn changes and the capture chain. `--padtest` drives a
virtual controller through the input layer and checks what comes out,
`--audiotest` measures every sound, and `--divedump` prints each attack curve as
a polyline. `--stage N`, `--players N`, `--autofire`, `--observe`, `--mute` and
`--seed N` set a run up; several of the wave figures are worst-of over a
run, so they want sampling across seeds rather than reading once.
`claudaga --help` lists the lot.

The commit history carries the reasoning behind most of what is here: why the
paths are shaped as they are, which measurement caught which bug, and what was
tried and reverted.
