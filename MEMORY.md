# Claudaga — project state

Updated 2026-08-26. Newest state first; trim entries that stop mattering.

## Where things stand

- **v1.0.2 released, repo public**, zlib licence (Zoid Kirsch and contributors;
  Vincent Janelle contributed the GitHub Actions workflows). Audio is CC0 with
  assets/audio/CREDITS.txt. All regression checks green; no TODOs in the tree.
- Gameplay is considered tuned and fair by Zoid through the bonus rounds and
  the fly-in. One deliberate deferral remains (below).

## Open items

1. **Ordinary-stage difficulty pass** — deferred by Zoid, never done. Known
   facts for whoever picks it up: the ramp flattens at stage 16 (16 and 24 are
   identical); the diver cap is never reached, so it is not a live knob —
   burst length is; sample with `--seed 0..7`, worst-of metrics lie on one run.
2. Binaries are unsigned; SmartScreen warns on the installer. Accepted — README
   tells people to click through. Revisit only if distribution widens.
3. High-score initials deliberately skipped; a single best score is recorded.

## Decisions and why (the ones that constrain future work)

- **Fairness is rule-driven, not tuned.** Missiles guarantee 28 ticks of
  warning (time, not height — 44px sounded safe and was a reaction-time trap).
  Attacks may cross the fighter's row but never sideways faster than the
  fighter moves (AIM_SLOPE; relaxing to 2x bought ~1 convoy tick in 15, so it
  stays at 1.0). Entry paths never enter at the fighter's altitude.
- **Bonus rounds:** two groups at a time with a pause between pairs (arcade
  rule); rounds 2-4 use column-shaped lanes E/F, translated not mirrored, so
  where to stand must be learnt (round 1 stays beatable from the middle — it
  teaches the format); every lane arrives over the top (lane C entered from a
  side edge at fighter height and was removed for it, along with unused D);
  flyers arrive no faster than the gun cycles (CHAL_MIN_GAP = FIRE_COOLDOWN).
- **Music** (bonus.wav) is a 22.86s cut of CC0 "Venus" so no round loops it;
  tools/make_bonus_music.py is the recipe — the source is a 5MB download not
  worth committing, and not writing the recipe down cost two re-downloads.
- **Icon** is rendered from SHP_FIGHTER by the game itself (`--icon`); the .ico
  is committed because the exe cannot link an icon that does not exist yet.
- **Music is stage state, not a start-of-stage event** (game_stage_music) —
  three routes reached a stage without starting one and arrived silent.
- **package.bat verifies its staged files** before zipping — it once shipped a
  zip with no exe and said OK (failed copies do not stop a batch file).
- **Comments trimmed twice** to "what the code cannot say, once" (910 -> ~700
  lines). Do not let histories creep back in; they belong in commit messages.

## Bugs whose shape may recur (worth recognising)

- **State handed a position it was not at:** the fly-in warp family — stale
  lane_dx after a bonus round, joins starting at the path end instead of the
  enemy, sway applied in one space but not the other. Detector: sharpest
  change of speed in --stats.
- **A schedule that outruns a capability:** bonus flyers every 9 ticks vs a
  12-tick gun. Totals looked fine; moment-to-moment was impossible. Check
  rates against each other, not budgets.
- **Aliasing:** challenge stages every 4th stage vs 4 entry sets meant one set
  never played. Anything cycled by stage number wants a co-prime check.
- **Metrics measuring the wrong thing:** off-screen convoys, crossings read as
  convoys, a "jump" that was legitimate escort geometry, a harness that muted
  audio being asked if music played. Validate the detector against the bug.
- **Scripted-edit debris:** a repair script once wrote `' + BS + '` into a C
  string (survived because that error path never printed). After bulk edits,
  grep for leaked artifacts.

## Harness quick reference

`--stage N` start there (bonus = 3,7,11,15,...) · `--dual` paired fighter ·
`--seed N` shift the wave RNG · `--stats N` run headless N ticks and report ·
`--chaltrack` dump bonus flyer positions per tick · `--icon out.ico` ·
`--players 2` · `--observe` invulnerable · `--trace` state transitions.
Scratch analysis scripts (solve/budget/measure) were session-local and are
gone; the committed tool is tools/make_bonus_music.py. Rebuild analysis from
--chaltrack dumps if needed.
