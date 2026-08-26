#ifndef CLAUDAGA_AUDIO_H
#define CLAUDAGA_AUDIO_H

#include <stdbool.h>

/* Sound, through SDL_mixer. Two rules, both about what it may not do.
 *
 * Never load-bearing: every entry point works whether or not a device opened or
 * a file was found, so there is no return value to check and a machine with no
 * sound card plays in silence.
 *
 * Never perturbs the simulation: the variant picker has its own generator, or
 * muting the game would change which enemy attacked next and every headless
 * measurement would be of a different game.
 *
 * Files are found via SDL_GetBasePath, not the working directory. */

typedef enum {
    SFX_SHOT,        /* the fighter fires                      */
    SFX_ENEMY_FIRE,  /* a diver fires                          */
    SFX_DIVE,        /* an enemy peels out of formation        */
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

/* Sets the two levels, each 0..VOLUME_STEPS. Effects and music are separate
   because they are competing for the same ear: the music is the thing you turn
   down to hear the game, and the effects are the thing you turn down when
   somebody else is in the room. */
void audio_set_levels(int sfx, int music);

/* Silences everything without losing its place, for a paused game. Music
   resumes where it stopped rather than restarting, which a halt would not. */
void audio_pause(bool paused);

/* Fades the current track out over a few hundred milliseconds. A cut to
   silence at the end of a bonus round is more noticeable than the music. */
void audio_music_stop(void);

/* Reports what the mixer can actually do at once, and measures every sound it
   loaded: length, peak, loudness, and how much of the energy sits down at the
   bottom. Balancing a mix by ear is not available here, so it is done by
   number instead. With a directory it measures that instead of the game's own
   sounds, which is how a candidate is judged before it is adopted.

   Returns non-zero if something could not be measured. */
int audio_report(const char *dir);

#endif /* CLAUDAGA_AUDIO_H */
