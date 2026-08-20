# Claudaga

A Galaga clone in C99 on SDL2, built with MSVC. The sprite sheet is loaded and
fully indexed, the rotation system is in and verified, the attack wave flies its
scripted entry into formation, and once assembled it attacks — enemies dive at
the player, fire back, leave the screen, and come round again. Shots kill,
collisions kill, the score counts, and clearing a wave starts the next stage.

## Build and run

The repository does not carry its dependencies, so fetch them once after
cloning:

```bash
tools\fetch_deps.bat
```

That pulls SDL2 2.32.10 and stb_image.h into `third_party/`. Both versions are
pinned, and SDL2 in particular ships prebuilt `.lib` and `.dll` files, so an
unplanned upgrade would change what actually links. Re-running it is harmless -
it skips anything already there. Then:

```bash
build.bat
```

`build.bat` finds Visual Studio through `vswhere`, enters the x64 MSVC
environment, compiles everything in `src/`, and copies `SDL2.dll` next to the
executable. Pass `release` for an optimised, windowed build; the default is a
debug build on the console subsystem so `printf` and asserts are visible.

Compiler output is teed to `build/build.log` and the final line reports a
warning count — `[build] OK with 2 warning(s)` rather than a bare `OK`. The
build is `/W4` and currently warning-free; the count exists because warnings
scroll off the top and a build that only says "OK" reads as clean when it is
not.

Run it from the project root, since the sheet is loaded by relative path:

```bash
build\claudaga.exe
```

`Tab` cycles the three views.

| View | Keys |
| --- | --- |
| Sprite browser | `←` `→` page through all 30 groups |
| Pose check | `←` `→` change sprite |
| Play | `←` `→` move, `Space` fire, `R` restart the game, `P` show paths and headings, `A` toggle attacks |

`Esc` quits.

Flags: `--scene` / `--pose` pick a starting view, `--page N` and `--subject N`
pick within it, `--scale N` sets the window zoom (default 3), `--paths` turns on
the path overlay, `--at TICK` fast-forwards the simulation before the first
frame, `--shot out.bmp` renders one frame and exits, `--stats N` runs the wave
headless for N ticks and reports which enemies attacked and how close any two
flying the same path came to each other, and `--trace` logs each stage
handover.

`--trace` reports each stage handover: what was still in the air when the stage
ended, and whether the new wave arrives with stray shots or already-damaged
bosses. It is the direct way to check a stage starts clean, and the "still in
the air" count is what shows the drop is doing something — over a long run it is
usually zero but not always, and the cases where it is not are exactly the ones
that used to freeze on screen.

Two more flags exist purely to make the game inspectable without a pair of hands
on the keyboard. `--observe` stops the fighter dying, so a long `--at` run reaches
the tick it was asked for instead of restarting halfway. `--autofire` holds down
fire and sweeps the ship side to side; parked in the middle it only ever shoots
up one column, so without the sweep a wave never clears and the later stages
cannot be reached to look at.

`--at` with `--shot` is the workhorse for checking animation: it dumps any
chosen moment of the entry to a bitmap without needing to catch it by eye.

A note if you build from a bash shell rather than `cmd` or PowerShell: MSYS
ships a `find` that shadows the Windows one on PATH, and Windows' `find` syntax
handed to it reads as directories to search. `build.bat` avoids `find`
entirely for that reason.

## Layout

```
src/
  common.h      screen geometry, Vec2, heading constants
  gfx.*         window, renderer, PNG loading, blitting, screenshots
  atlas.*       sprite rects, frame counts, and the heading to pose mapping
  path.*        Catmull-Rom splines resampled by arc length
  formation.*   slots, entry flights, attacks, formation flight, state machine
  fx.*          explosion animations and the score popups
  game.*        the fighter, its shots, the starfield, collision, score, stages
  debugfont.*   5x7 font for debug overlays
  main.c        the loop and the three views
assets/       galaga_sheet.png
third_party/  SDL2 2.32.10 (VC dev libs), stb_image.h - fetched, not committed
tools/        fetch_deps.bat    - downloads the two dependencies
              inspect_sheet.py  - re-derives sprite coordinates from the sheet
```

The picture is 224x288, the arcade's 288x224 raster turned upright for a
vertical cabinet. `SDL_RenderSetLogicalSize` plus integer scaling blows it up to
the window without ever making one game pixel taller than its neighbour.

## The sprite sheet

`assets/galaga_sheet.png` is the Spriters Resource rip credited on the sheet
itself to 125scratch, xdonthave1xx, and Goemar. It is a 458x256 palette PNG.
Two palette entries matter: index 0 is the grey gutter between blocks, and
index 1 is the black the hardware used as transparent, which
`gfx_load_texture` punches out to alpha 0 on load.

Most art sits on a regular grid — a 1px border, then 16x16 cells every 18px —
which is what the `GX`/`GY` macros in `atlas.c` compute. The exceptions keep
their own spacing: the 32x32 explosions step 34px, the 48x80 tractor beam steps
50px, and the HUD art is not on any grid at all, so those rects are measured ink
bounds.

| Group | Frames | Notes |
| --- | --- | --- |
| Fighter, and the captured red one | 7 each | pure rotation |
| Boss Galaga, two colour states | 8 each | it takes two hits and swaps palette in between |
| Butterfly (Goei), Bee (Zako) | 8 each | |
| Challenging-stage flyers | 6–8 each | sixteen of them, unnamed on the sheet, so numbered `BONUS_1`…`BONUS_16` |
| Player explosion | 4 | 32x32 |
| Enemy explosion | 5 | 32x32 |
| Tractor beam | 3 | 48x80 |
| Player missile | 1 | |
| Enemy missile | 8 | one per heading |
| Score values | 8 | 150, 400, 800, 1000, 1500, 1600, 2000, 3000 |
| Stage flags | 6 | 1, 5, 10, 20, 30, 50 |
| Life icon | 1 | |

The enemy missiles are laid out as a 3x3 rose whose cells point the way the shot
travels, so `MissileDir` is the sheet's own geometry rather than an invention.
The middle of that rose is not a ninth heading — it is the player's shot.

`atlas.c` is hand-written rather than generated. `tools/inspect_sheet.py`
re-derives the coordinates from the image so the table can be checked; run it if
the sheet is ever replaced.

## Rotation

Every flyer stores the same thing: **seven frames turning counter-clockwise from
north (frame 6) to west (frame 0), 15 degrees apart.** That is one quadrant.
Mirroring it horizontally, vertically, or both covers the other three, which is
exactly the trick the arcade hardware used, and it is what `atlas_pose` does.
The enemies carry an eighth frame that is a second north-facing pose; frames 6
and 7 are the pair they alternate to flap their wings in formation.

This was established rather than assumed: rotating frame 6 counter-clockwise by
90 degrees reproduces frame 0 for the fighter, bee, butterfly, and boss alike.
The **pose check** view then verifies the mapping end to end by drawing a sprite
at 24 headings, each placed in the direction it claims to face — every copy
should point straight out from the centre, and any quadrant or mirror mistake
shows up immediately as one pointing the wrong way.

A consequence worth knowing: the sheet's natural orientation is north, so
enemies parked in formation are drawn *unflipped* and therefore face away from
the player. They turn to face where they are going as soon as they move.

## Formation entry

Forty enemies fill a 10x5 grid: four Boss Galaga, sixteen butterflies, twenty
bees. They arrive in five flights of eight, each flight split into two streams
that enter from opposite sides at once. Slots are numbered rank by rank and
flights take them eight at a time, which lands the bosses and the first
butterflies in the opening flight, as on the real board.

Entry paths are Catmull-Rom splines through hand-placed control points, baked
once at startup into a dense polyline with running arc length. That resampling
is what makes the rest simple: an enemy advances a fixed number of pixels per
tick regardless of how the curve bunches up, and its heading is just the tangent
at that distance. Two paths are authored — a dive down the middle into a low
loop, and a sweep up the left edge into a loop across the top — and each is
mirrored for the opposite stream.

Both paths stop *below* the formation, because that is where the eight enemies
sharing a flight have to stop sharing. From there each one flies its own **cubic
Hermite curve** into its slot. Hermite is the right tool because it is defined
by its endpoint tangents: the departure tangent is the path's own exit
direction, so the join continues the flight instead of kinking away from it, and
the arrival tangent is pinned due north, so the enemy comes to rest already
facing the way it will sit. The heading is read off that curve's tangent rather
than interpolated separately, which is what keeps the enemy pointing where it is
actually travelling. The approach is timed from the curve's measured length, so
every enemy crosses at flight speed and decelerates to a stop no matter how far
its slot is. Approaching every slot from beneath is also what lets the arrival
tangent point north without the curve hooking back on itself.

Each enemy runs a four-state machine: waiting off-screen, entering along its
path, flying its join curve into its slot, then parked and flapping. `P` draws
the paths, and a whisker on each enemy showing its heading.

## Dive attacks

Once the wave is assembled it starts attacking, roughly every 105 ticks. The
check that the formation is complete has to latch — the instant the first enemy
leaves its slot the formation is no longer complete, so testing it each tick
fires one attack and then never another.

A dive path is built where the enemy is sitting rather than from a fixed
template: it peels out of the slot, curls over, and sweeps down across the
player's column before leaving the bottom of the screen. Aiming the lower half
at the player is what stops a run of attacks from tracing the same line every
time. Because each dive needs its own path, they come from a small pool that is
handed out and returned.

A boss attacks with the two parked butterflies nearest its column, and the trio
flies as a real formation rather than as three separate launches: they share one
path and take fixed stations on it, the escorts a set distance ahead and to
either side, the boss at the trailing apex of the triangle. Escorts in front is
how the arcade flies it.

Sharing the path is what holds the shape through the turns. All three advance
the same distance each tick and sample the path at their own offset, with the
sideways offset applied along the path's normal — so the triangle banks with the
flight instead of being a screen-space offset that stops making sense the moment
the path bends. An escort does not begin on station, it begins wherever it was
parked, so it eases across during the first stretch of the dive, which reads as
the trio forming up as it drops out of the formation. The path is reference
counted, because an escort flying ahead finishes before its boss and must not
pull the path out from under it.

Coming home is deliberately not a special case. A diver leaves the bottom of the
screen, re-enters over the top on a return path that drops down the *outside* of
the formation, and turns back up beneath it — which leaves it in exactly the
state an arriving enemy is in, so the existing entry state and join curve finish
the trip. One new state covers attacking; the rest was already there.

Each return path is a lane with a **departure queue**. Enemies do not join it
the moment they finish a dive — they take the next free slot, at least
`RETURN_SPACING` ticks after the last one. Without that, a dive group re-entered
stacked: the two escorts share a station and so finish on the very same tick, and
starting both at the head of the same path left them exactly coincident the whole
way home. A queue fixes it for any two enemies that happen to coincide rather
than just for escorts, and the waiting happens off-screen where nothing is drawn.
`--stats` reports the closest two enemies ever came while sharing a path; it
reads 42px with the queue and 0px without.

Who attacks is weighted by type rather than picked uniformly, and the reason is
worth recording. Picking uniformly from every parked enemy means any one of the
twenty bees almost never gets a turn; preferring a boss outright is worse, since
with four of them one is nearly always available, and the bottom two ranks then
never move at all. `--stats` measures the result directly, because a
distribution is not something a screenshot can show. As it stands, over 20 000
ticks, per slot: boss 12.0 dives, butterfly 9.4, bee 4.0 — an individual boss
attacks about three times as often as an individual bee, which is a skew in the
right direction rather than a rout.

## Combat

Collision is a distance test rather than a box, and all the radii are tighter
than half a sprite because the art inside a 16x16 cell is smaller than the cell.
The arcade is forgiving about near misses and matching that matters more than
being geometrically exact.

Scoring follows the arcade: a bee is 50 parked and 100 diving, a butterfly 80
and 160. A Boss Galaga survives the first hit and changes colour, and is worth
150 parked. Killed mid-dive it pays for the company it kept — 400 alone, 800
with one escort still flying, 1600 with both — and because a dive group shares
one path, counting the survivors is just a matter of asking who else is on it.
Those four values are exactly the boss-tier sprites the sheet carries, so the
kill pops the real artwork up where the enemy died rather than drawn text.

Divers fire back and the missile picks its sprite from the direction rose by
velocity. Parked enemies never shoot.

Aim is deliberately *not* a straight line to the fighter, and this is a fairness
rule rather than a stylistic one. The fighter is pinned to a single row and can
only move along it, so a missile arriving flat along that row cannot be dodged
at all — and an enemy that had swooped down level with the fighter was firing
almost exactly that, at up to 78 degrees off vertical. Two rules keep a shot
fair: the enemy must be some way above the fighter's row to take one, and the
aim is clamped into a 45-degree cone about straight down, so every missile has
enough downward travel to step aside from. `--stats` reports the steepest shot
fired over a run; it reads 37 degrees with the rules in and 78 without.

One consequence: with the cone in place only the south, south-east and
south-west frames of the direction rose ever get used in play. The rest are
still indexed, and still visible in the sprite browser.

Clearing every enemy pauses briefly and sends in the next stage. Nothing else
about a stage changes yet — the attack rate and speeds are fixed.

Only **one player missile** may be in the air at a time. Missing therefore
costs real time, since the shot has to clear the top of the screen before
another can be fired, and that is where most of the difficulty comes from. The
arcade allowed two; one is a deliberate choice here, and `MAX_SHOTS` is the only
thing to change.

Everything in the air is dropped the moment a stage is cleared — player shots
and enemy missiles both. Enemy missiles especially have to go *then* rather than
when the next wave arrives: the wave stops being updated while the message is
up, so a missile still flying would hang motionless on screen for the full two
seconds. It also stops a bullet from an already-dead enemy taking a life during
the congratulations.

Player shots are advanced *before* the between-stage and game-over early
returns, and cleared outright when a new wave is handed out. Both matter, and
the reason is worth keeping: when the shot update sat after those returns, a
shot still in the air as the last enemy died simply stopped, hung motionless on
screen for the whole pause, and then carried into the next wave still live —
where it hit whatever flew past it, so fresh Boss Galagas turned up already
damaged. One leak, two symptoms, and the second one looked convincingly like a
failed state reset. `--trace` prints the state of every new wave for exactly
this reason; it reports stray shots and pre-damaged bosses at each stage
boundary, and reads zero for both.

### The wave has its own random generator

Worth recording because it was a measurement bug, not just a style choice. The
wave used to draw from the global `rand()`, which the starfield also draws from
about a hundred times a tick, and which enemy fire then started drawing from per
diver per tick. That made the attack mix depend on how much *other* code had
consumed that frame — so the distribution `--stats` measured with the wave
running alone was not the distribution the game actually played, and the numbers
shifted whenever anything unrelated changed. The wave now owns an xorshift32
seeded once at startup. `--stats` is reproducible run to run, measures what the
game does, and the per-slot mix lands at boss 8.8 dives, butterfly 7.9, bee 4.5
— roughly 2:1.8:1, which is what the type weights were designed for.

## Things assumed, not verified

- **The two Boss Galaga colours are its damage states, green first.** The
  shapes are identical and only the palette differs, which fits a two-hit
  enemy, but the sheet does not say which colour is the damaged one. The game
  currently starts them green and turns them blue on the first hit; if that is
  backwards it is a one-line swap in `wave_hit`.
- **Frames 6 and 7 are a wing-flap pair.** Both are verifiably north-facing
  poses; that the game alternates them in formation is the natural reading, not
  something the sheet states.
- **The challenging-stage flyers are numbered, not named**, and the six-frame
  rows among them are treated as a shorter sweep over the same quadrant. None
  are used in a stage yet, so that reading is untested.
- **The entry paths are authored by eye.** They reproduce the shape and feel of
  Galaga's entries — the streams, the splits, the loops — but they are not the
  arcade's path tables, and the timings are tuned to look right rather than
  measured.

### On splines, historically

Catmull and Rom published their splines in 1974 and Galaga shipped in 1981, so
the maths is not an anachronism — but the *approach* is. A 1981 arcade board was
not evaluating cubic polynomials per enemy per frame. Motion like this was done
with incremental turn-rate tables: hold a heading, apply a fixed angular delta
for so many frames. That is why the arcade's loops come out as clean circular
arcs, and why the sprite's rotation index falls straight out of the motion
instead of being recovered from a tangent the way it is here. Splines were
chosen because they are pleasant to author and tune, not because they are what
the hardware did. Worth knowing if the goal ever shifts from *looks like Galaga*
to *behaves like Galaga*.

## Next

The tractor beam capture and the dual fighter are the marquee mechanic still
missing, and the 48x80 beam animation is indexed and unused waiting for it.
Alongside that: the formation's side-to-side breathing, a difficulty ramp so the
attack rate climbs with the stage rather than sitting fixed, and the challenging
stages, which is what the sixteen unnamed bonus flyers are for.

Smaller gaps: the score uses the debug font because the sheet carries no
alphabet, so a proper HUD needs the arcade font ripped; there is no sound; and
the extra-life award at 20,000 points is not implemented.
