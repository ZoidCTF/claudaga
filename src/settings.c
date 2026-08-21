#include "settings.h"

#include <stdio.h>
#include <string.h>

#include <SDL.h>

static bool settings_path(char *out, size_t n)
{
    char *pref = SDL_GetPrefPath("Claudaga", "Claudaga");
    if (!pref) { out[0] = 0; return false; }
    int written = SDL_snprintf(out, n, "%ssettings", pref);
    SDL_free(pref);
    return written > 0 && (size_t)written < n;
}

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

bool settings_load(Settings *s)
{
    s->sfx        = SETTINGS_SFX_DEFAULT;
    s->music      = SETTINGS_MUSIC_DEFAULT;
    s->fullscreen = false;

    char path[512];
    if (!settings_path(path, sizeof path)) return false;

    FILE *f = fopen(path, "r");
    if (!f) return false;

    /* Read as key and value rather than by position, so a file written by an
       older build - or one missing a line someone deleted - still yields
       whatever it does carry, and the rest stay at their defaults. */
    char key[32];
    int  value;
    while (fscanf(f, "%31s %d", key, &value) == 2) {
        if      (!strcmp(key, "sfx"))        s->sfx        = clamp(value, 0, VOLUME_STEPS);
        else if (!strcmp(key, "music"))      s->music      = clamp(value, 0, VOLUME_STEPS);
        else if (!strcmp(key, "fullscreen")) s->fullscreen = value != 0;
    }
    fclose(f);
    return true;
}

void settings_save(const Settings *s)
{
    char path[512];
    if (!settings_path(path, sizeof path)) return;

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "sfx %d\n",        clamp(s->sfx, 0, VOLUME_STEPS));
    fprintf(f, "music %d\n",      clamp(s->music, 0, VOLUME_STEPS));
    fprintf(f, "fullscreen %d\n", s->fullscreen ? 1 : 0);
    fclose(f);
}
