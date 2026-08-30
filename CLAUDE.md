# Claudaga — instructions for Claude

A Galaga clone in C99 on SDL2, Windows/MSVC. 224x288 logical picture, 60 ticks
per second, everything on screen generated (polygons, stroke font, per-frame
geometry — nothing loads an image). Public repo: github.com/ZoidCTF/claudaga,
zlib licence. Current state and open items: @MEMORY.md

## Build

```
tools\fetch_deps.bat          once after cloning (SDL2 + SDL2_mixer, pinned)
build.bat [debug|release]     debug = console subsystem + tools; release = /O2 /DNDEBUG
tools\package.bat 1.2.3       clean release -> dist\claudaga-1.2.3.zip
```

- From a bash-spawned shell, always `cmd //c "cd /d F:\Projects\Claudaga & call .\build.bat debug"` —
  the sandbox sets NoDefaultCurrentDirectoryInExePath, so bare `build.bat` is not found.
- Policy: zero warnings at /W4. Before any commit, both configs must build clean
  from an empty tree (`rm -rf build` first).
- The repo lives on a NAS share (10GbE SMB). That is deliberate; do not suggest
  moving it local.

## Verify before committing (the regression suite)

The game is its own test harness — headless flags, no framework. Minimum bar:

```
claudaga --padtest                                          input selftest
claudaga --shot x.bmp --players N --at 20000 --autofire --observe --trace
        for N=1,2; grep "starts", every line must say "damaged bosses 0"
claudaga --shot x.bmp --stage 12 --stats 4000               wave metrics
claudaga --shot x.bmp --stage 3 --stats 2500                bonus-round metrics
claudaga --audiotest                                        expect 22 of 22 loaded
--title --options --paused --shapes --pose --demo           each renders a shot
```

Several wave figures are worst-of-run: one seed is a sample, not a number.
Tune against `--seed 0..7` across stages, never a single run.

Invariants the stats must keep reporting healthy:
- sideways crossing of the fighter's row <= 1.5 px/tick (fighter moves 1.6)
- entry paths clear the fighter's row by ~24 px; no path enters from a side edge
- missile warning >= 28 ticks (FIRE_MIN_WARNING — written as time, not height)
- bonus rounds: max 2 groups / 10 flyers on screen; every lane arrives over the
  top; within_gap >= FIRE_COOLDOWN; rounds 2-4 reach only 4 of 8 groups from
  one column but every pair has a column reaching both lanes
- sharpest change of speed stays near dive speed (a spike = a position handed
  across a state change without being carried)

## Methodology (how this project is worked on)

1. Build the detector first, and validate it catches the bug before fixing —
   reinstate the bug if needed. A metric that flags everything gets ignored.
2. When a metric and the code disagree, suspect the metric (off-screen convoys,
   the 0.3px crossing, the "jump" that was escort geometry).
3. Reason about geometry with pictures or dumped polylines, not control points —
   reading control points has repeatedly failed to catch curves that double back.
4. Measure before and after; put the numbers in the commit message.

## Editing gotchas (these have each cost real time)

- Bash heredocs eat backslashes: `\n` inside a heredoc'd Python string arrives
  as a real newline and corrupts C strings. Write edit scripts to a file with
  the Write tool using raw strings; repair breakage with chr(92) tricks.
- .bat files are CRLF (enforced by .gitattributes); edit them with
  newline='' handling, never universal newlines.
- After a scripted multi-edit partially fails, `git checkout` the file and
  re-run — re-applying doubles the edits that did land.

## Conventions

- Comments say what the code cannot say about itself, once. No histories, no
  measurement logs — those live in commit messages. Longest block ~16 lines.
- Commit messages are prose: what, why, what was measured, what was verified.
- Commit locally as work completes; NEVER push until Zoid says to push.
- Tuning constants live in formation.c next to the ramp; derived rules
  (FIRE_MIN_HEIGHT, AIM_SLOPE, CHAL_MIN_GAP) are written as the constraint,
  with the pixel value computed from it.

## Release

Tag `vX.Y.Z` and push the tag -> CI builds zip + Inno installer and publishes
the GitHub release (~40s). `gh workflow run "Windows release" -f version=X.Y.Z-test`
for a dry run (artifact only, nothing published). Regenerate the icon with
`claudaga --icon res\claudaga.ico` only when the fighter artwork changes;
re-cut the bonus music with tools/make_bonus_music.py (source URL inside).
