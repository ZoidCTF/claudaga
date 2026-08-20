#ifndef CLAUDAGA_AUDIO_H
#define CLAUDAGA_AUDIO_H

#include <stdbool.h>

/* Sound, through SDL_mixer.
 *
 * Two rules shape this module, and both are about what sound is *not* allowed
 * to do.
 *
 * It is never load-bearing. Every entry point here works whether or not a
 * device opened, whether or not SDL_mixer initialised, and whether or not a
 * single file was found - callers do not check a return value and there is
 * nothing to check. A machine with no sound card plays the game in silence
 * rather than refusing to start, and a missing file costs one effect rather
 * than the run.
 *
 * And it never perturbs the simulation. The variant picker draws from its own
 * generator, for exactly the reason the wave has one: if it shared a generator
 * with anything the game reads, muting the game would silently change which
 * enemy attacked next, and every headless measurement in this project would be
 * measuring a different game from the one being played.
 *
 * Files are found relative to the executable via SDL_GetBasePath rather than
 * the working directory, so the game can still be launched from anywhere. */

typedef enum {
    SFX_SHOT,        /* the fighter fires                      */
    SFX_ENEMY_FIRE,  /* a diver fires                          */
    SFX_ENEMY_DIE,
    SFX_BOSS_HIT,    /* a Boss Galaga that survived the hit    */
    SFX_PLAYER_DIE,
    SFX_BEAM,        /* a tractor beam opens                   */
    SFX_STAGE,       /* a stage begins                         */
    SFX_EXTRA,       /* an extra fighter is awarded            */
    SFX_PERFECT,     /* a bonus round caught all forty         */
    SFX_COUNT
} SfxId;

typedef enum {
    MUSIC_TITLE,
    MUSIC_BONUS,
    MUSIC_COUNT
} MusicId;

/* `enabled` false skips the device entirely: nothing is opened, nothing is
   loaded, and every call below becomes a no-op. That is what the headless
   harness uses - a screenshot run has no business opening an audio device,
   and a --stats run would otherwise fire thousands of effects at a device
   nobody is listening to. */
void audio_init(bool enabled);
void audio_shutdown(void);

/* True when a device is open and at least one file loaded. Only the startup
   banner uses this; the game itself never asks. */
bool audio_ok(void);

/* Plays one of the effect's variants, chosen at random. Several sounds have
   two or three near-identical takes, which is what stops a rapid sequence of
   the same event - four enemies dying inside a second - turning into an
   obvious machine-gun repeat of one sample. */
void audio_play(SfxId id);

/* Music loops until stopped. Asking for the track already playing does
   nothing, so this can be called every frame from a view that does not track
   what it started. */
void audio_music(MusicId id);

/* Fades the current track out over a few hundred milliseconds. A cut to
   silence at the end of a bonus round is more noticeable than the music. */
void audio_music_stop(void);

#endif /* CLAUDAGA_AUDIO_H */
