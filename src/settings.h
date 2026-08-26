#ifndef CLAUDAGA_SETTINGS_H
#define CLAUDAGA_SETTINGS_H

#include <stdbool.h>

/* What a player may change, kept beside the high score under SDL_GetPrefPath
 * because next to the executable is often unwritable. Two files, not one: they
 * are written at different moments, and sharing would mean one rewriting the
 * other from a stale copy. Every failure is silent. */

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
