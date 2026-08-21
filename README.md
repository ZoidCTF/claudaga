# Claudaga

A Galaga clone in C99 on SDL2, built with MSVC. Every pixel it draws is
generated — polygons for the artwork, strokes for the text, per-frame geometry
for the explosions and the tractor beam. It loads no images and ships no
artwork.

The attack wave flies a scripted entry into formation and then attacks — enemies dive, fire back, leave
the screen and come round again. Shots kill, collisions kill, the score counts,
and clearing a wave starts the next stage.

A Boss Galaga will drop out of formation and try to take your fighter with a
tractor beam; shoot it and you get the fighter back as a second hull. Every
fourth stage from the third is a challenging stage.

## Build and run

The repository does not carry its dependencies, so fetch them once after
cloning:

```bash
tools\fetch_deps.bat
```

That pulls SDL2 2.32.10 and SDL2_mixer 2.8.2 into `third_party/`. Both versions
are pinned: they ship prebuilt `.lib` and `.dll` files, so an unplanned upgrade
would change what actually links. Only `SDL2_mixer.dll` is taken from the mixer
archive — the optional decoder DLLs beside it are for formats this game does not
use, and Vorbis and MP3 are handled by code built into SDL2_mixer itself.
Re-running it is harmless — it skips anything already there.
Then:

```bash
build.bat
```

`build.bat` finds Visual Studio through `vswhere`, enters the x64 MSVC
environment, compiles everything in `src/`, and copies both DLLs and the
`assets/` folder next to the executable. Pass `release` for an optimised, windowed build; the default is a
debug build on the console subsystem so `printf` and asserts are visible.

Compiler output is teed to `build/build.log` and the final line reports a
warning count — `[build] OK with 2 warning(s)` rather than a bare `OK`. The
build is `/W4` and currently warning-free; the count exists because warnings
scroll off the top and a build that only says "OK" reads as clean when it is
not.

Run it from anywhere — the audio is found relative to the executable through
`SDL_GetBasePath` rather than the working directory, which is why `build.bat`
stages it there. The one file it writes is the high score, which lives wherever
`SDL_GetPrefPath` says user data belongs:

```bash
build\claudaga.exe
```

It opens on the title screen: `↑` `↓` to choose, `Enter` to confirm. `Tab`
cycles to the game and the three tools behind it.

| View | Keys |
| --- | --- |
| Title *(default)* | `↑` `↓` select, `Enter` confirm |
| Play | `←` `→` move, `Space` fire, `R` restart, `P` show paths and headings, `A` toggle attacks |
| Shape browser | the vector artwork and the font, at a size where they can be judged |
| Pose check | `←` `→` change shape |

`Esc` always heads back towards the menu and never ends the process: out of
Options first, then out of whatever view is up. The only ways out are the QUIT
item and closing the window.

Running out of lives returns to the title rather than restarting. The game does
not restart itself — it reports that it is finished and lets whatever is driving
it decide, which is how the menu gets a look in. A headless run has nobody to
show a menu to, so `--at` starts a fresh game instead; both go through the same
function, so the two cannot drift apart.

Flags: `--title` / `--scene` / `--pose` / `--shapes` pick a starting view — and
`--at` or `--stats` without one implies the game, since there is nothing to
fast-forward on a menu. Others: `--subject N` picks within it, `--scale N` sets
the window zoom (default 3), `--paths` turns on the path overlay, `--at TICK`
fast-forwards the simulation before the first frame, `--shot out.bmp` renders
one frame and exits, `--stats N` runs the wave headless for N ticks and reports
what the stage's difficulty works out to and how the wave behaved under it,
`--stage N` starts on a later stage so that difficulty can be measured without
playing up to it, `--mute` opens no audio device, `--padtest` runs the
controller self test and exits, `--options` opens on the options page so it can
be screenshot, `--audiotest [DIR]` reports the mixer's capacity and measures
every sound, and `--trace` logs each stage handover.
`--shot` and `--stats` mute themselves.

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
  gfx.*         window, renderer, screenshots
  shape.*       vector artwork: the renderer and the transform
  shapes.c      the artwork itself, as polygons
  font.*        stroke font: glyphs as line segments, drawn with thickness
  path.*        Catmull-Rom splines resampled by arc length
  formation.*   slots, entry flights, attacks, formation flight, state machine
  fx.*          explosion animations and the score popups
  game.*        the fighter, its shots, the starfield, collision, score, stages
  main.c        the loop and the three views
third_party/  SDL2 2.32.10 (VC dev libs) - fetched, not committed
tools/        fetch_deps.bat - downloads SDL2
```

The picture is 224x288, the arcade's 288x224 raster turned upright for a
vertical cabinet. `SDL_RenderSetLogicalSize` keeps the game's coordinates — and
therefore its paths, speeds and collision radii — in those units whatever the
window size. Integer scaling used to be on, because a fractional zoom made some
rows of raster pixels taller than others; with geometry there is nothing to
align, so it is off and the picture is drawn at the window's real resolution.

## Vector artwork

Every flyer, shot and explosion is geometry, fed to `SDL_RenderGeometry` as
coloured triangles. No new dependency, and three things fall out of it that the
sheet could not give:

- **Resolution independence.** The game is drawn at the window's true size
  instead of an integer multiple of 224x288.
- **Continuous rotation.** A shape simply turns to its heading. The sheet stored
  seven frames covering one quadrant and mirrored them for the rest, which is
  what the hardware did in 1981; that whole mapping is retired.
- **Free recolouring.** A damaged Boss Galaga and the captured red fighter are
  palette swaps of the same drawing. On the sheet each needed a second full set
  of frames.

Shapes are authored in `shapes.c` in the units the game already thinks in: one
unit is one pixel of the 224x288 picture, the origin is the sprite's centre, and
the artwork faces north. They occupy the same 16x16 the sheet's cells did, so
the collision radii tuned against those cells still hold and nothing about
gameplay shifted.

Two conventions do most of the work. Only the **right half** of each design is
written out, with a `mirror` flag drawing it again flipped — which halves the
typing and makes the symmetry exact rather than something to get right twice.
And every polygon is a **convex fan**, which is a constraint on the artwork
rather than a limitation worth engineering around: a concave wing is two convex
pieces, and keeping the renderer a fan means there is no triangulator to write
or get wrong.

The explosions are generated rather than drawn. As raster art they were four and
five fixed frames; as geometry it costs less and reads better to throw shards
outward and let them fade, each blast carrying a seed so two deaths in the same
spot are not the same picture. Score popups are drawn as text, which covers any
value rather than only the boss tiers the sheet had sprites for.

Formation enemies used to flap by alternating two drawn poses. With a shape that
can scale freely, a slow pulse reads as the same thing and needs no second
drawing.

### The font

Text is a stroke font: each glyph is a handful of line segments, drawn with real
thickness as geometry. It replaced a 5x7 bitmap that scaled by turning every
pixel into a bigger square, and looked increasingly out of place beside artwork
that had gone smooth.

The metrics are deliberately the ones the bitmap had, so every caller and every
piece of layout arithmetic still lines up. The style is squared off, because
curves cost segments and read no better at this size, and stroke ends are
extended by the half-width so corners close up instead of leaving a notch —
cheaper than mitring the joint and, at this size, indistinguishable from it.

The shape browser draws the whole alphabet, which is the only practical way to
judge glyphs.

## Where the artwork came from

The project began by indexing a ripped arcade sprite sheet — a 458x256 palette
PNG, thirty groups of frames, coordinates derived from the image rather than
eyeballed. All of it has since been replaced by generated geometry, and the
sheet, its atlas, the browser that displayed it and the image loader that read
it have all been removed. Nothing in the repository is anyone else's artwork,
and nothing loads a file to draw with.

What the sheet held and what replaced it:

| Sheet group | Now |
| --- | --- |
| Fighter, and the captured red one | one shape, two palettes |
| Boss Galaga, two colour states | one shape, two palettes |
| Butterfly, Bee | shapes of their own |
| Player and enemy explosions | procedural shards |
| Tractor beam | a procedural cone |
| Player and enemy missiles | shapes of their own |
| Score values | drawn as text |
| Stage flags | one shield, six palettes |
| Life icon | the fighter, scaled down |
| Sixteen challenging-stage flyers | four shapes, cycled |

Only the last row is not one for one, and that is a choice rather than a debt:
the arcade fields sixteen sets, this fields four, and adding more means drawing
more shapes.

The designs are loosely after the originals rather than traced from them.

## Rotation

Shapes rotate continuously, so there is nothing to map. The **pose check** view
draws one at 24 headings, each placed in the direction it claims to face, and
every copy should point straight out from the centre like a spoke. That check
mattered a great deal against the sheet, where a heading had to be resolved to
one of seven stored frames plus a choice of mirrorings and any slip put a ship
on backwards; against geometry it is close to a formality, but it is what would
catch a sign error in the rotation matrix and it costs nothing to keep.

For the record, since the mapping is gone from the code: the sheet stored seven
frames turning counter-clockwise from north (frame 6) to west (frame 0), 15
degrees apart — one quadrant, mirrored horizontally, vertically or both to reach
the other three, exactly as the arcade hardware did it. That was established
rather than assumed, by rotating frame 6 counter-clockwise by 90 degrees and
finding it reproduced frame 0 for the fighter, bee, butterfly and boss alike.

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
The value is drawn where the enemy died.

Divers fire back, the missile turning to point along its velocity. Parked
enemies never shoot.

Aim is deliberately *not* a straight line to the fighter, and this is a fairness
rule rather than a stylistic one. The fighter is pinned to a single row and can
only move along it, so a missile arriving flat along that row cannot be dodged
at all — and an enemy that had swooped down level with the fighter was firing
almost exactly that, at up to 78 degrees off vertical. Two rules keep a shot
fair: the enemy must be some way above the fighter's row to take one, and the
aim is clamped into a 45-degree cone about straight down, so every missile has
enough downward travel to step aside from. `--stats` reports the steepest shot
fired over a run; it reads 45 degrees with the rules in - the clamp binding at
exactly its ceiling - and 78 without.

Clearing every enemy pauses briefly and sends in the next stage.

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

### The fighter keeps flying between stages

The pause after a wave is cleared used to take the controls away, because the
player update sat below an early return. It reads as the game hanging. Movement
and firing now happen above that return, and anything fired during the pause is
cleared when the next wave is handed out, so nothing leaks across the boundary -
which was the whole reason the shot handling moved above the early returns in
the first place.

### Dying clears the board

When the fighter is destroyed every diver is recalled to its return lane, the
missiles in the air are dropped, and no new attack launches until there is a
ship to attack again. The respawn additionally waits for the spot to be clear
rather than only for the explosion to finish.

Without that, dying and reappearing straight into an enemy still mid-dive cost
the next life immediately. Measured with a stationary fighter, which is the
failing case - the autofire harness sweeps side to side and moves off the spawn
point before anything can reach it, so it did not reproduce the bug at all.
Standing still over 25,000 ticks the shortest gap between deaths was 85 ticks
before the change and 139 after, against a respawn wait of 70: the 85 was a ship
that lasted fifteen ticks.

## Capture and the dual fighter

Instead of diving, a boss sometimes descends, hangs over the fighter and opens a
tractor beam. Caught, the fighter is drawn up the cone tumbling, costs a life,
and rides home with its captor in enemy colours.

**The captor climbs back to its slot from where it hangs**, rather than going
round by the return lane a diver uses. That distinction matters more than it
sounds: a diver ends its run off the bottom of the screen and genuinely has to
come back in over the top, but a boss that has been hovering below the formation
the whole time has not gone anywhere. Sending it round the houses made it vanish
and reappear in the top corner with the stolen fighter in tow. It now leaves on
the heading it is already flying — south — so the join curve swings it down and
around before it climbs, and it turns rather than flipping about-face in a single
frame.

### Where the captive rides, and why it is a target

The captured fighter sits **directly above** its captor in screen space, always
— parked, entering, or attacking. Screen space rather than rotated with the boss
is what makes it read as *above* and not as *behind the nose*: a diving boss is
travelling down the screen, so above is behind, and the pair descends as a
vertical column with the stolen fighter trailing. That is what the arcade does.

The captive is a target in its own right. Hit it and it is gone for good, and it
pays nothing, because it is your own ship.

Which of the two a shot can reach falls out of the offset, and it is worth being
explicit about the direction because getting it backwards is easy. Screen y grows
downward: the fighter's row is at y=264 and the formation is at y=44, and a shot
climbs by *decreasing* y. So a rising shot meets the larger y first, which is the
boss. The boss is between the player and the captive the whole way, and a straight
shot up the column frees the fighter rather than destroying it. Losing the captive
is what happens when the pair is not lined up with you — which is most of the
time, since they are moving.

An earlier version of this had the reasoning inverted and pushed the captive out
to the boss's side while diving, to "open a shot" that was never closed. The
symptom was visible before the argument was: the pair flew side by side instead
of in column.

### The reunion is played out, not fitted in

Freeing the fighter stops the game rather than letting the join happen in
traffic. The board is recalled, no new attack launches, the surviving fighter is
walked to the middle of its row, and the freed one flies down to that same fixed
point to meet it. Control comes back when they dock.

Both halves have to arrive before they join. The rescued fighter gets there
first as often as not, and docking with a ship still sliding across would snap it
the rest of the way. Steering during any of it would let the player pull away
from the very thing flying down to meet them, which is why the controls are held
rather than merely ignored.

A dual fighter is two hulls, not one wide one: both are real targets, both
shoot, and it is allowed twice as many shots in the air, which is most of what
makes the rescue worth attempting. A hit takes the rescued hull rather than a
life, so the pair buys a mistake. One caught in a beam loses the wingman instead
of being captured.

The beam is geometry - bands sliding down a widening cone, where the movement is
the whole effect, since a static gradient reads as a shape rather than a beam.
Free recolouring is what makes the captive cheap: it is the fighter's own
artwork in the enemy palette, turned to face the way its captors do.

Hover height and beam length have to be chosen together. The first attempt
hovered at y=104 with a beam 78 long, which stops eighty pixels short of the
fighter's row at 264, so no capture was possible at all.

One thing reported alongside the teleport could not be reproduced: that the
replacement fighter waits for the whole formation to reassemble. Measured across
five stages it comes back after a fixed 140 ticks — 2.3 seconds — and the same
140 both before and after these fixes, because attacks are already paused while
there is nobody to attack and the board drains during the countdown. On a busier
board it stretches to about 200. The likeliest explanation is that the captor
disappearing into the corner made the wait *look* like it was waiting on the
formation. `--trace` now reports the gap, so if it happens again it will say so.

## Title screen

`CLAUDAGA` is drawn a letter at a time so the wordmark can run through a colour
ramp, with a dark pass offset behind it for depth, which is most of what makes
an arcade logo look like one rather than like a line of text. The ramp is a warm
arch, brightest in the middle: a version that ran all the way to cool put a pale
blue letter on the end of the word and broke it.

The menu is Start, Options and Quit, with the fighter itself as the cursor. The
starfield is the game's own, shared rather than duplicated - two star systems
drifting at different rates would be obvious the moment the game started.

Options is a placeholder. It is reachable from the title rather than sitting in
the `Tab` rotation, and says so.

Running out of lives comes back here. Silently dropping the player into a fresh
stage 1 reads as a glitch rather than as an ending.

## Stage flags

The count along the bottom right is spelled out with the largest denominations
first, as the arcade does: stage 23 is a twenty and three ones. They are laid
out from the right edge inwards so the row grows leftwards, and capped at what
the row will hold.

One shield shape drawn six ways. On the cabinet these are six separate pieces of
art, but the whole job of a flag is to be told apart from the others at a
glance, and colour alone does that — so it is one drawing and six palettes.

## Difficulty, and the shape of an attack

Every stage used to be stage one: the attack interval, the dive speed and the
missile rate were compile-time constants and the wave never saw the stage
number. It does now, and works out six numbers once when the stage starts —
interval, dive speed, missile rate, burst length, a ceiling on how many attacks
may be in the air, and the sway period. Stage 1 and stage 12 are the two ends;
anything between is interpolated and anything past 12 sits at the hard end.

The interesting part is what *didn't* work. Raising the cap on simultaneous
attacks changed nothing whatsoever, and `--stats` said so plainly: the peak sat
at two dive groups at every stage from 1 to 20, however high the ceiling went. A
dive lasts about 150 ticks and the stage-1 interval is 105, so two is simply
what fits — the cap was never the binding constraint, the interval was, and
shortening the interval far enough to change that would leave no lulls at all.

So the shape changed instead of the rate. An attack event now launches a
**burst** — one attack at stage 1, up to four by stage 12, arriving 24 ticks
apart and then followed by a full interval of quiet. That is harder to dodge
than a metronome and easier to read. Measured over 6000 ticks, stages 1 / 8 / 20
give peaks of 2 / 3 / 4 dive groups and 65 / 158 / 209 dives.

The ceiling, to be straight about it, is never actually reached at those
numbers — peak 4 against a cap of 6 — so it currently does nothing. It is kept
as a bound on the burst rather than deleted, since burst length is the knob most
likely to be turned up next and the alternative is the dive-path pool running
dry deciding what happens instead.

Bursts reintroduced an old bug, and did it quietly. A dive breaks towards its
nearer edge, so two leaders parked on the same side of the formation fly
near-identical curves — the same "two enemies on top of each other" the entry
lanes needed a queue to fix. Two things keep them apart: the 22-tick gap, and
picking each follow-up attacker from the *opposite* half of the formation. That
filter has to be applied before the attacker's type is chosen rather than after,
or a burst landing on the bosses — four of them, often all on one side — finds
nobody eligible and falls straight back to the near half, which is exactly the
case that goes wrong.

### Measuring "flying together" is not measuring distance

The first attempt at a detector took the closest two enemies in different dive
groups ever came. It read **0.3px**, which sounds alarming and means nothing:
groups on genuinely different curves *cross* each other all the time, and a
crossing puts them at zero for an instant. Minimum distance cannot tell a
crossing from a convoy.

What separates them is duration, so what is tracked is the longest run of
consecutive ticks a pair of groups stays within 14px. A crossing is two or three
ticks; anything travelling together is dozens. Swept across stages 2 to 24:

| burst gap | worst run | mean | stages over the 20-tick line |
|---|---|---|---|
| 16 ticks | 27 | 12.2 | 2 |
| 22 ticks | 18 | 4.2 | 0 |
| 24 ticks | 9 | 0.7 | 0 |
| 28 ticks | 3 | 0.3 | 0 |

24 is the cheap step: the worst case halves for about a tenth of the dive count.
Past that a burst starts spanning longer than the interval that follows it,
which is an evenly spaced stream with extra steps.

The opposite-side rule is *not* load-bearing, and the sweep says so — without it
the worst run at the same 24-tick gap is 16 rather than 9, still under the line.
It is kept because halving the residue is cheap, and because a burst that
alternates sides fans out across the screen instead of arriving out of one half
of the formation, which is worth having on its own.

### The formation sways

The parked block drifts ten pixels either side of its slots, on a cycle that
tightens from 420 ticks at stage 1 to 240 at the top of the ramp. Amplitude is
fixed rather than ramped, and by the screen rather than by taste: the outer
columns sit 40px from the edge and a 16px sprite needs 8 of that.

One offset moves the entire formation, so the block never distorts — columns
stay columns, which is how the arcade does it. The subtlety is the arriving
enemy. Its join curve is built against the slot's *home*, and the sway is added
to the curve's output rather than to its endpoint, so the block can move
underneath an approaching enemy without the curve needing to be rebuilt and
without the arrival snapping — which is the same "weird slide into position"
that the Hermite join was introduced to kill in the first place. The sine starts
at zero, so it is also motionless while the wave is still flying in.

`--stats` reports how far parked enemies were actually seen sitting from their
slots, taken from the enemies rather than from the offset: it reads -10.0 to
+10.0.

## Controllers

The game does not read a keyboard. It reads three booleans — left, right, fire —
and `input.c` is the only thing that knows where they came from. That is what
stops "and now the same for a controller" from meaning a second `if` beside
every existing one, and it is also what lets the headless harness drive the game
by filling the struct in directly instead of forging a scancode array, which is
what it used to do.

Two kinds of input live there and they are not interchangeable. **Flying is
sampled**: what matters is whether left is held right now. **Menus are
edge-triggered**: holding a direction should move the cursor once, not sixty
times a second. The keyboard gets its edges free from `SDL_KEYDOWN`, but a stick
pushed to one side is a level rather than an event, so those edges have to be
manufactured — a press threshold at 18000 and a release threshold at 9000, with
the gap between them there to stop a stick resting near the line from chattering
the cursor. A single threshold would do exactly that.

The menu itself is four functions — up, down, confirm, back — that both the
keyboard and the pad call. Written twice, one copy acquires a case the other
lacks the first time anything is added; it is the same reasoning that put the
warm-up and the interactive loop through one `play_tick`.

Fire is any of the four face buttons or the right trigger. Which button is *the*
button is a matter of what somebody grew up holding, and none of the four is
needed for anything else in flight.

### The options page, and a screen nobody looked at

The controls summary shipped four pixels too wide. Two centred lines, the longer
of them 38 characters, and 38 characters at a six pixel advance is 228 against a
224 pixel screen — so `(GAME_W - font_width(s)) / 2` went negative and both ends
were clipped.

It shipped because there was no way to draw it except by clicking through the
menu, and every other screen in the project has a flag that renders it. `--options`
is that flag now, and the layout is three columns rather than two sentences, so
no arrangement of the strings can run off the edge and the two rows line up —
which separately centred lines never do. Measured after the change, the block
spans x=29.3 to x=194.0 with margins of 29.3 and 30.0.

The rest of the text screens were swept for the same fault at the same time —
title, options, results, play and a bonus round — and all sit clear of both
edges.

### Testing it without hands

Controller support is the one thing here that cannot be checked by running the
game and looking, because checking it means somebody holding a pad. SDL can
attach a **virtual controller** and have its axes and buttons set from code, so
`--padtest` drives one through the real open, event, sample and close paths —
not a mock of them — and reports 21 checks.

It earned its place on the first run, with three failures that looked like three
different problems and were one. A controller present at startup was being
opened *twice*: once by the enumeration in `input_open`, and again by the
`DEVICEADDED` event SDL queues for devices that were already there. The wrong
count was the harmless part. The second handle was closed by the first unplug,
which left the first handle dangling and being read every frame. Nothing about
the symptoms pointed at it.

The fourth failure was the test's own fault, and is worth writing down because
it is genuinely surprising: **a virtual trigger at rest is raw −32768, not raw
0.** The mapping normalises a full-range axis on to 0..32767, so raw 0 arrives
as 16383 — half pressed, which fires, and correctly so. The test released the
trigger to 0 and then reported the game as broken for doing the right thing
with it.

## Sound

Sound arrived last, and is the only part of the game that is not generated from
first principles. It runs through SDL_mixer at 22050 Hz — the rate the assets
are, and the rate a board of this vintage would have run at anyway — with a
512-frame buffer. That last number is not arbitrary: it is about 23ms, and at
60Hz a shot whose sound lands two frames after the trigger reads as lag rather
than as a shot.

Two rules shape `audio.c`, and both are about what sound is not allowed to do.

**It is never load-bearing.** Every entry point works whether or not a device
opened, whether or not SDL_mixer initialised, and whether or not a single file
was found. Callers do not check a return value because there is nothing to
check. A machine with no sound card plays in silence rather than refusing to
start, and a missing file costs one effect rather than the run.

**It never perturbs the simulation.** The variant picker draws from its own
generator, for precisely the reason the wave has one — if it shared with
anything the game reads, muting would silently change which enemy attacked next
and every headless measurement in this project would be measuring a different
game from the one being played. That is verified rather than asserted: a 6000
tick traced run produces byte-identical events with sound on and with `--mute`.

Several effects have two or three near-identical takes and one is chosen at
random per play, which is what stops four enemies dying inside a second from
turning into an obvious machine-gun repeat of one sample. **Sixteen effects mix
at once**, with music on its own stream on top; past sixteen an effect is
dropped rather than stealing a channel, since the fullest moment in the mix is
exactly where a cut-off sample sounds worst. That is measured, not assumed —
`--audiotest` starts twenty and counts what sounds.

### Balancing a mix you cannot hear

Sample packs do not arrive at matched loudness, and picking between them by ear
is not available here. So `--audiotest` measures each sound instead: length,
peak, RMS, and the share of its loudness below 320 Hz — which is what "boomy"
actually means, and separates a chest-thump from a chiptune zap far better than
peak level does.

It described the explosions accurately: `explosionCrunch_000/002/003` run
**0.78 to 1.55 seconds with 63 to 94 percent of their energy under 320 Hz** — a
long bass thump, forty times a stage, against a player shot of 0.24s.

It did not follow that swapping them was the answer, and that is the useful part
of the story. A replacement set was chosen on those numbers — the `phaserDown`
family, 0.31 to 0.50 seconds at 47 to 62 percent, peaking a third lower — and it
sounded worse. The measurements were right about *why* the originals were
fatiguing and gave no opinion at all about whether the alternatives were any
good, which is not something they can tell you. The crunches are back, played at
half volume, which was the fix all along.

Measuring a mix you cannot hear narrows the search. It does not make the
choice.

The same numbers did turn up a second problem nobody had reported: the three
jingles peaked at 0.13 to 0.45 against effects peaking at 0.90 — up to
seventeen decibels down, and simply lost under the shooting. `Mix_VolumeChunk`
can only attenuate, so the only way up was in the files, which are normalised
to a peak of 0.80.

Each effect now carries its own level in the table, applied at load. The
report prints it beside the sample figures on purpose: the volume does not
touch the audio data, so peak and RMS describe the file and the level sits on
top of them, and reading the two apart gives the wrong picture of the mix.

### Where it came from, and what is wrong with it

Everything is CC0, credited in `assets/audio/CREDITS.txt`: the effects are from
Kenney's *Sci-fi Sounds*, the title music is Joth's *8Bit Title Screen*, and the
bonus round and jingles come from SketchyLogic's *NES Shooter Music*. The
jingles and the bonus track were downsampled to 22050 Hz mono, and the bonus
track was cut to fifteen seconds — roughly the length of a challenging stage —
at the quietest point in a 13.5-to-17-second window, which lands on a phrase
boundary more often than a round number would, with a quarter-second fade over
the cut.

None of it is Galaga's audio and none of it is a transcription; that music and
those sounds are Namco's. The chiptune was picked to sit in the same idiom.

Worth saying plainly, though: **the effects are still the weakest part of the
project.** They are modern, sample-based, slightly reverberant sci-fi sounds,
and Galaga's were pure square and noise waveforms off a 1981 Namco WSG — short,
dry and harsh. Sitting next to artwork that was deliberately rebuilt to match
the original's proportions, they are the one element that gives away that it is
a modern game. Generating them instead, the way the sprites were replaced by
polygons, would be maybe two hundred lines and would sound materially more
correct. The plumbing above does not care where a `Mix_Chunk` came from, so that
swap is cheap whenever it is wanted.

## Scoring, and what a run is worth

Kills pay on the arcade's scale, and a Boss Galaga killed mid-dive pays for the
company it kept. On top of that sit three things that make a run mean something
beyond the current stage.

**Extra fighters** arrive on the arcade's default schedule: the first at 20,000
points, the second at 70,000, and one more every 70,000 after that. The award
loops rather than testing a single threshold, because a perfect bonus round pays
10,000 in one go and could otherwise step straight over one. Every path that
adds to the score goes through a single `add_score`, which is what makes it
impossible to add points at a new call site and quietly forget the check.
Verified by tracing a long run: the awards land at 20,080 and at exactly 70,000,
with the next threshold moving to 140,000. The spare-ship row only holds five,
so past that the count keeps rising and the display does not.

**The high score** sits centred at the top of the HUD, between the player's
score and the stage, the way the cabinet arranges it. It outlives the process:
it is the one file this game touches, written wherever `SDL_GetPrefPath` says
user data belongs rather than next to the executable, which is often somewhere
unwritable. Every failure reading or writing it is silent on purpose — a game
that will not start because it could not read a high score is a worse game than
one that forgets. It is loaded once in `game_init` and deliberately *after* the
first `game_restart`, since a restart is a new game rather than a new machine.

**The results screen** ends a game the way the arcade does — shots fired, hits
landed, and the ratio between them — which needs the misses counted too, so the
tally is taken at the muzzle rather than at the target. A boss that survives a
hit still counts as a hit. It is a more honest summary of a run than the score
is: a player who reached stage 8 by spraying and one who reached it by aiming
score about the same, and only this tells them apart.

The board freezes behind the panel rather than carrying on, because a wave still
flying underneath the numbers reads as though the game were not over. Fire cuts
the screen short, but only once it has been released first — without that,
holding the trigger as the last fighter dies, which is exactly what a player is
doing at that moment, would skip the screen before it drew a frame. The headless
harness holds fire permanently, which verifies that guard and only that guard;
the skip itself is exercised by hand.

Adding the screen turned up one thing missing from the stroke font, which had no
`%`. It has one now — the two rings are short strokes rather than drawn boxes,
since at five pixels wide a ring closes into a blob anyway.

## Challenging stages

Every fourth stage from the third - 3, 7, 11 - is a bonus round. Forty flyers
cross the screen in eight groups of five along paths that begin and end
off-screen; nothing forms up, nothing attacks, nothing fires, and nothing can
hurt the fighter. Anything not shot on the way through simply escapes.

They reuse the entry machinery exactly: a challenging-stage flyer runs the same
`ENEMY_ENTERING` state along the same kind of path, and the only difference is
what happens when the path runs out - a normal enemy peels into its slot, a
bonus one is gone.

Catching one pays 100; catching all forty pays a further 10,000, deliberately
worth far more than the sum of the hits so the round is something to be good at
rather than a free forty shots.

Four rounds cycle, and they differ in pattern rather than only in colour. There
are four passes to draw on — the original S-shaped descent and the climb out
through the top, plus a lateral crossing that serpentines straight across the
middle of the screen without descending at all, and a corkscrew that turns twice
in opposite directions on its way out of the bottom. Each round flies two of
them, mirrored, so the screen is crossed from four directions; each also sets
its own group spacing and speed, and gets a further speed multiplier from the
stage number so that cycling back round to the first pattern at stage 19 is not
a step backwards. The four flyers — moth, scorpion, dart and orb — ride along
with the rounds.

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

- **A damaged Boss Galaga turns from green to blue.** The sheet carried two
  palettes for the same shape, which fits a two-hit enemy, but never said which
  was the damaged one. Now that the colours are ours the question is only which
  reads better; swapping them is a one-line change in the draw.
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

Separate effect and music volumes, which the Options screen has room for and
reports nothing about yet. That is the list.
