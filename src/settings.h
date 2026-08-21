#ifndef CLAUDAGA_SETTINGS_H
#define CLAUDAGA_SETTINGS_H

#include <stdbool.h>

/* The handful of things a player is allowed to change, and the file they live
 * in.
 *
 * It goes beside the high score, in whatever directory SDL_GetPrefPath names,
 * for the same reason: next to the executable is often somewhere unwritable.
 * Two files rather than one because they are written at different moments -
 * the high score when a game ends, these the instant they are changed - and
 * sharing a file would mean one of them rewriting the other's value from a
 * stale copy.
 *
 * Every failure is silent. A game that will not start because it could not
 * read a volume is worse than one that starts at the default. */

#define VOLUME_STEPS 10

typedef struct {
    int  sfx;          /* 0..VOLUME_STEPS */
    int  music;        /* 0..VOLUME_STEPS */
    bool fullscreen;
} Settings;

/* Defaults chosen to land on the mix the game shipped with, so that a player
   who never opens the options page hears exactly what they always did. */
#define SETTINGS_SFX_DEFAULT   6
#define SETTINGS_MUSIC_DEFAULT 4

/* Fills `s` with the defaults, then whatever the file overrides. Returns
   false when there was no file to read, which is how a fresh install knows to
   write one out. */
bool settings_load(Settings *s);
void settings_save(const Settings *s);

#endif /* CLAUDAGA_SETTINGS_H */
