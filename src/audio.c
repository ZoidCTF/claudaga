#include "audio.h"
#include "settings.h"

#include <math.h>
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

/* The two levels, as a fraction of MIX_MAX_VOLUME. A channel's final level is
   its chunk volume times its channel volume, so setting every channel here
   scales the whole effect bus while the per-effect trims below keep their
   relationship to each other. */
#define FADE_MS      400

#define MAX_VARIANTS 3

typedef struct {
    const char *file[MAX_VARIANTS];   /* NULL terminates the list */
    int         vol;                  /* 0..MIX_MAX_VOLUME, per effect */
} SfxDef;

/* The effects, and which files each draws from. Written out rather than
   generated from a stem and a count because the extensions differ - the
   Kenney effects are Vorbis and the jingles are WAV - and because a table you
   can read is worth more here than one you can loop over. */
/* Each effect carries its own level, applied on top of the effect bus. Full is
   MIX_MAX_VOLUME, which is what an effect got before any of this existed, so
   a table of full values plays exactly as the game always did and anything
   below it is a deliberate trim of one sound.

   Only the explosion is trimmed, and only because it was asked for: the
   Kenney crunches are long and bass-heavy - 0.78 to 1.55 seconds with 63 to
   94 percent of their energy under 320 Hz, forty times a stage - and half
   volume is what makes them sit under the game rather than on top of it. The
   samples themselves are the originals; a replacement set was tried and was
   worse.

   Note this can only attenuate. Anything too quiet has to be lifted in the
   file itself, which is what happened to the jingles: they arrived peaking at
   0.13 against effects peaking at 0.9, seventeen decibels down and inaudible
   under the shooting. */
#define VOL_FULL   MIX_MAX_VOLUME
#define VOL_SOFT   (MIX_MAX_VOLUME * 5 / 8)
#define VOL_HALF   (MIX_MAX_VOLUME / 2)
#define VOL_SUBTLE (MIX_MAX_VOLUME * 3 / 8)

static const SfxDef SFX_FILES[SFX_COUNT] = {
    [SFX_SHOT]       = { { "shot_0.ogg",    "shot_1.ogg",    "shot_2.ogg" },  VOL_FULL },
    [SFX_ENEMY_FIRE] = { { "efire_0.ogg",   "efire_1.ogg",   "efire_2.ogg" }, VOL_FULL },

    /* The swoop as an enemy peels out of formation. Quiet on purpose: one
       dive is meant to be noticed rather than announced, and at higher stages
       a burst puts three or four of them in the air a few frames apart, which
       is the point of the sound. Three takes of slightly different lengths so
       that overlap does not come out as one sound played louder. */
    [SFX_DIVE]       = { { "swoop_0.ogg",   "swoop_1.ogg",   "swoop_2.ogg" }, VOL_SUBTLE },
    [SFX_ENEMY_DIE]  = { { "boom_0.ogg",    "boom_1.ogg",    "boom_2.ogg" },  VOL_HALF },
    [SFX_BOSS_HIT]   = { { "bosshit_0.ogg", "bosshit_1.ogg", NULL },          VOL_FULL },
    [SFX_PLAYER_DIE] = { { "die_0.ogg",     "die_1.ogg",     NULL },          VOL_FULL },
    [SFX_BEAM]       = { { "beam_0.ogg",    NULL,            NULL },          VOL_FULL },
    [SFX_STAGE]      = { { "jingle_stage.wav",   NULL, NULL },                VOL_SOFT },
    [SFX_EXTRA]      = { { "jingle_extra.wav",   NULL, NULL },                VOL_FULL },
    [SFX_PERFECT]    = { { "jingle_perfect.wav", NULL, NULL },                VOL_FULL },
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
    s_on = true;
    audio_set_levels(SETTINGS_SFX_DEFAULT, SETTINGS_MUSIC_DEFAULT);

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
            int vol = SFX_FILES[id].vol;
            Mix_VolumeChunk(c, vol > 0 ? vol : MIX_MAX_VOLUME);

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

void audio_set_levels(int sfx, int music)
{
    if (!s_on) return;
    if (sfx   < 0) sfx   = 0;  if (sfx   > VOLUME_STEPS) sfx   = VOLUME_STEPS;
    if (music < 0) music = 0;  if (music > VOLUME_STEPS) music = VOLUME_STEPS;

    Mix_Volume(-1, MIX_MAX_VOLUME * sfx / VOLUME_STEPS);
    Mix_VolumeMusic(MIX_MAX_VOLUME * music / VOLUME_STEPS);
}

void audio_pause(bool paused)
{
    if (!s_on) return;
    if (paused) {
        Mix_Pause(-1);
        Mix_PauseMusic();
    } else {
        Mix_Resume(-1);
        Mix_ResumeMusic();
    }
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

/* --------------------------------------------------------------- measuring */

/* A one-pole lowpass, used to ask how much of a sound is bass. The corner is
   deliberately low: "boomy" means energy under a few hundred Hz, and the ratio
   of lowpassed loudness to total loudness separates a chest-thump from a
   chiptune zap far better than peak level does. */
#define BASS_HZ 320.0f

typedef struct {
    float secs;
    float peak;    /* 0..1 */
    float rms;     /* 0..1 */
    float bass;    /* 0..1, share of loudness below BASS_HZ */
} SoundStats;

static bool measure(Mix_Chunk *c, SoundStats *st)
{
    int freq = 0, channels = 0;
    Uint16 fmt = 0;
    if (!Mix_QuerySpec(&freq, &fmt, &channels)) return false;
    if (fmt != AUDIO_S16SYS || channels < 1) return false;

    const Sint16 *p = (const Sint16 *)c->abuf;
    int frames = (int)(c->alen / (sizeof(Sint16) * (size_t)channels));
    if (frames <= 0) return false;

    /* One pole: y += k * (x - y), with k set from the corner frequency. */
    float k = 1.0f - expf(-2.0f * (float)M_PI * BASS_HZ / (float)freq);
    float lp = 0.0f;
    double sum = 0.0, sum_lp = 0.0;
    float peak = 0.0f;

    for (int i = 0; i < frames; ++i) {
        /* Mono sum, so a sound panned either way measures the same. */
        float v = 0.0f;
        for (int ch = 0; ch < channels; ++ch) v += p[i * channels + ch];
        v /= (float)channels * 32768.0f;

        float a = fabsf(v);
        if (a > peak) peak = a;

        lp += k * (v - lp);
        sum    += (double)v * v;
        sum_lp += (double)lp * lp;
    }

    st->secs = (float)frames / (float)freq;
    st->peak = peak;
    st->rms  = (float)sqrt(sum / frames);
    st->bass = st->rms > 0.0f ? (float)(sqrt(sum_lp / frames) / st->rms) : 0.0f;
    return true;
}

/* `vol` is the level the effect is played at, or -1 for a candidate that has
   not been adopted and so has none. It is worth printing beside the sample
   figures because Mix_VolumeChunk does not touch the audio itself - peak and
   rms below describe the file, and the level is applied on top of them, so
   reading the two apart would give the wrong picture of the mix. */
static void report_one(const char *label, Mix_Chunk *c, int vol)
{
    SoundStats st;
    if (!c || !measure(c, &st)) {
        printf("  %-22s could not be measured\n", label);
        return;
    }
    if (vol >= 0) {
        printf("  %-22s %5.2fs  peak %.2f  rms %.3f  bass %.2f  vol %3d\n",
               label, st.secs, st.peak, st.rms, st.bass, vol);
    } else {
        printf("  %-22s %5.2fs  peak %.2f  rms %.3f  bass %.2f\n",
               label, st.secs, st.peak, st.rms, st.bass);
    }
}

/* How many effects the mixer will carry at once, measured rather than assumed:
   start as many as there are channels and count what is still sounding. */
static void report_overlap(void)
{
    Mix_Chunk *c = NULL;
    for (int id = 0; id < SFX_COUNT && !c; ++id) {
        if (s_variants[id] > 0) c = s_sfx[id][0];
    }
    if (!c) { printf("  no sound to test overlap with\n"); return; }

    Mix_HaltChannel(-1);
    int started = 0;
    for (int i = 0; i < AUDIO_CHANNELS + 4; ++i) {
        if (Mix_PlayChannel(-1, c, 0) >= 0) ++started;
    }
    int sounding = Mix_Playing(-1);
    printf("  effect bus level          %d of %d\n",
           Mix_Volume(0, -1), MIX_MAX_VOLUME);
    printf("  music level               %d of %d\n",
           Mix_VolumeMusic(-1), MIX_MAX_VOLUME);
    printf("  channels allocated        %d\n", AUDIO_CHANNELS);
    printf("  starts accepted           %d of %d attempted\n",
           started, AUDIO_CHANNELS + 4);
    printf("  sounding at once          %d\n", sounding);
    printf("  music is separate         %s\n",
           Mix_PlayingMusic() ? "yes (playing)" : "yes (its own stream)");
    Mix_HaltChannel(-1);
}

int audio_report(const char *dir)
{
    if (!s_on) { printf("audio is not open\n"); return 1; }

    printf("overlap\n");
    report_overlap();

    printf("\nsounds\n");

    if (!dir) {
        for (int id = 0; id < SFX_COUNT; ++id) {
            for (int v = 0; v < s_variants[id]; ++v) {
                report_one(SFX_FILES[id].file[v], s_sfx[id][v],
                           SFX_FILES[id].vol > 0 ? SFX_FILES[id].vol
                                                 : MIX_MAX_VOLUME);
            }
        }
        return 0;
    }

    /* A directory of candidates. Loaded and freed one at a time rather than
       held, since a pack can be a hundred files and none of them are wanted
       yet. */
    /* SDL 2 has no directory listing, so the caller leaves a list.txt beside
       the candidates: one name per line. Cheaper than pulling in a platform
       header for a tool that runs a handful of times. */
    char listpath[1200];
    SDL_snprintf(listpath, sizeof listpath, "%s/list.txt", dir);
    FILE *f = fopen(listpath, "r");
    if (!f) {
        printf("  no %s - write one name per line\n", listpath);
        return 1;
    }

    char line[256];
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (n == 0) continue;

        char full[1400];
        SDL_snprintf(full, sizeof full, "%s/%s", dir, line);
        Mix_Chunk *c = Mix_LoadWAV(full);
        report_one(line, c, -1);
        if (c) Mix_FreeChunk(c);
    }
    fclose(f);
    return 0;
}
