#include "audio.h"

#include <stdio.h>
#include <string.h>

#include <SDL.h>
#include <SDL_mixer.h>

/* 22050 Hz is what the assets are, and it is the rate an arcade board of this
   vintage would have been running anyway. 512 frames is about 23ms of latency,
   which matters at 60Hz: a shot that arrives two frames after the trigger
   reads as lag rather than as sound. */
#define AUDIO_RATE     22050
#define AUDIO_BUFFER   512
#define AUDIO_CHANNELS 16

#define SFX_VOLUME   (MIX_MAX_VOLUME * 3 / 5)
#define MUSIC_VOLUME (MIX_MAX_VOLUME * 2 / 5)
#define FADE_MS      400

#define MAX_VARIANTS 3

typedef struct {
    const char *file[MAX_VARIANTS];   /* NULL terminates the list */
} SfxDef;

/* The effects, and which files each draws from. Written out rather than
   generated from a stem and a count because the extensions differ - the
   Kenney effects are Vorbis and the jingles are WAV - and because a table you
   can read is worth more here than one you can loop over. */
static const SfxDef SFX_FILES[SFX_COUNT] = {
    [SFX_SHOT]       = { { "shot_0.ogg",    "shot_1.ogg",    "shot_2.ogg" } },
    [SFX_ENEMY_FIRE] = { { "efire_0.ogg",   "efire_1.ogg",   "efire_2.ogg" } },
    [SFX_ENEMY_DIE]  = { { "boom_0.ogg",    "boom_1.ogg",    "boom_2.ogg" } },
    [SFX_BOSS_HIT]   = { { "bosshit_0.ogg", "bosshit_1.ogg", NULL } },
    [SFX_PLAYER_DIE] = { { "die_0.ogg",     "die_1.ogg",     NULL } },
    [SFX_BEAM]       = { { "beam_0.ogg",    NULL,            NULL } },
    [SFX_STAGE]      = { { "jingle_stage.wav",   NULL, NULL } },
    [SFX_EXTRA]      = { { "jingle_extra.wav",   NULL, NULL } },
    [SFX_PERFECT]    = { { "jingle_perfect.wav", NULL, NULL } },
};

static const char *MUSIC_FILES[MUSIC_COUNT] = {
    [MUSIC_TITLE] = "title.mp3",
    [MUSIC_BONUS] = "bonus.wav",
};

static bool       s_on;                              /* a device is open  */
static int        s_loaded;                          /* files that opened */
static Mix_Chunk *s_sfx[SFX_COUNT][MAX_VARIANTS];
static int        s_variants[SFX_COUNT];
static Mix_Music *s_music[MUSIC_COUNT];
static int        s_playing = -1;

/* The variant picker's own generator - see the header for why it is not
   allowed to share one with anything the game reads. */
static unsigned s_rng = 0xC1A0DA6Au;

static unsigned rng_next(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

/* Builds an absolute path to an audio file. Relative to the executable rather
   than the working directory: the game is launched from a shell, a shortcut
   and a debugger, and only one of those has a predictable cwd. */
static bool audio_path(char *out, size_t n, const char *file)
{
    char *base = SDL_GetBasePath();
    if (!base) return false;
    int written = SDL_snprintf(out, n, "%sassets/audio/%s", base, file);
    SDL_free(base);
    return written > 0 && (size_t)written < n;
}

void audio_init(bool enabled)
{
    if (!enabled) return;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        SDL_Log("audio: no audio subsystem (%s) - continuing in silence",
                SDL_GetError());
        return;
    }

    if (Mix_OpenAudio(AUDIO_RATE, MIX_DEFAULT_FORMAT, 2, AUDIO_BUFFER) < 0) {
        SDL_Log("audio: could not open a device (%s) - continuing in silence",
                Mix_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }

    Mix_AllocateChannels(AUDIO_CHANNELS);
    Mix_Volume(-1, SFX_VOLUME);
    Mix_VolumeMusic(MUSIC_VOLUME);
    s_on = true;

    char path[1024];
    int  expected = MUSIC_COUNT;
    for (int id = 0; id < SFX_COUNT; ++id) {
        for (int v = 0; v < MAX_VARIANTS && SFX_FILES[id].file[v]; ++v) ++expected;
    }

    for (int id = 0; id < SFX_COUNT; ++id) {
        for (int v = 0; v < MAX_VARIANTS; ++v) {
            const char *file = SFX_FILES[id].file[v];
            if (!file) break;
            if (!audio_path(path, sizeof path, file)) continue;

            Mix_Chunk *c = Mix_LoadWAV(path);
            if (!c) {
                SDL_Log("audio: %s did not load (%s)", file, Mix_GetError());
                continue;
            }
            s_sfx[id][s_variants[id]++] = c;
            ++s_loaded;
        }
    }

    for (int m = 0; m < MUSIC_COUNT; ++m) {
        if (!audio_path(path, sizeof path, MUSIC_FILES[m])) continue;
        s_music[m] = Mix_LoadMUS(path);
        if (!s_music[m]) {
            SDL_Log("audio: %s did not load (%s)", MUSIC_FILES[m], Mix_GetError());
            continue;
        }
        ++s_loaded;
    }

    /* Said out loud because a missing assets folder is otherwise completely
       silent - which is also exactly what a working mute looks like. */
    SDL_Log("audio: %d of %d file(s) loaded at %d Hz",
            s_loaded, expected, AUDIO_RATE);
}

void audio_shutdown(void)
{
    if (!s_on) return;

    Mix_HaltMusic();
    Mix_HaltChannel(-1);

    for (int id = 0; id < SFX_COUNT; ++id) {
        for (int v = 0; v < s_variants[id]; ++v) Mix_FreeChunk(s_sfx[id][v]);
        s_variants[id] = 0;
    }
    for (int m = 0; m < MUSIC_COUNT; ++m) {
        if (s_music[m]) Mix_FreeMusic(s_music[m]);
        s_music[m] = NULL;
    }

    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    s_on      = false;
    s_loaded  = 0;
    s_playing = -1;
}

bool audio_ok(void)
{
    return s_on && s_loaded > 0;
}

void audio_play(SfxId id)
{
    if (!s_on || id < 0 || id >= SFX_COUNT) return;

    int n = s_variants[id];
    if (n <= 0) return;   /* the files for this one never loaded */

    Mix_Chunk *c = s_sfx[id][rng_next() % (unsigned)n];

    /* -1 takes the first free channel. When every channel is busy the effect
       is dropped rather than cutting one off: forty enemies dying at once is
       the moment the mix is fullest, and stealing a channel there produces a
       clipped stutter that sounds worse than the missing copy. */
    Mix_PlayChannel(-1, c, 0);
}

void audio_music(MusicId id)
{
    if (!s_on || id < 0 || id >= MUSIC_COUNT) return;
    if (!s_music[id]) return;
    if (s_playing == (int)id && Mix_PlayingMusic()) return;

    if (Mix_PlayMusic(s_music[id], -1) < 0) {
        SDL_Log("audio: %s would not play (%s)", MUSIC_FILES[id], Mix_GetError());
        return;
    }
    s_playing = (int)id;
}

void audio_music_stop(void)
{
    if (!s_on) return;
    if (Mix_PlayingMusic()) Mix_FadeOutMusic(FADE_MS);
    s_playing = -1;
}
