#ifndef CLAUDAGA_GAME_H
#define CLAUDAGA_GAME_H

#include "gfx.h"
#include "formation.h"
#include "fx.h"
#include "input.h"

/* The playable scene: the starfield, the fighter and its shots, the attack
 * wave, and everything that happens when those things touch. It owns the
 * collision checks because it is the only thing that can see both sides. */

/* Shots in the air at once. Missing costs real time - a shot has to clear the
   top of the screen before its slot frees up - which is where most of the
   difficulty comes from. A dual fighter fires from both hulls and is allowed
   twice as many, which is most of what makes rescuing one worth doing. */
#define SHOTS_SINGLE 2
#define SHOTS_DUAL   4
#define MAX_SHOTS    SHOTS_DUAL
#define START_LIVES  3

/* How far each hull of a dual fighter sits from the pair's centre. */
#define DUAL_OFFSET 8.0f

typedef struct {
    Vec2 pos;
    bool alive;
} Shot;

typedef struct {
    float x;
    bool  alive;
    bool  dual;        /* a rescued fighter is flying alongside */
    int   lives;
    int   respawn;     /* ticks left before it comes back */

    /* Being drawn up a tractor beam. Not the same as dead: there is no wreck,
       the wave is not recalled, and what comes back is a fighter in enemy
       colours riding under the boss that took it. */
    bool  captured;
    Vec2  cap_pos;
    float cap_spin;
    int   cap_boss;
} Player;

typedef struct {
    Player player;
    Shot   shots[MAX_SHOTS];
    int    fire_cooldown;

    Wave   wave;
    Fx     fx;

    int    score;

    /* The best score this machine has seen. Kept out of game_restart so it
       survives a new game, and written to disk so it survives the process. */
    int    high_score;

    /* Score at which the next spare fighter is awarded. */
    int    next_life;
    int    extra_msg;     /* ticks left on the EXTRA FIGHTER notice */

    /* Every shot fired and every one that hit something, for the results
       screen. Galaga ends a game by telling you how well you shot rather than
       only how far you got, which means counting the misses too - so this is
       tallied at the muzzle, not at the target. */
    int    shots_fired;
    int    shots_hit;
    int    results;       /* ticks left on the results screen */
    bool   results_armed; /* fire has been released since the game ended */

    int    stage;

    /* Which stage a run begins on. Always 1 in play; --stage sets it so a
       later stage's difficulty can be measured without playing up to it. */
    int    first_stage;
    bool   always_dual;   /* every fighter launches paired, for testing */
    int    tick;
    int    stage_clear;   /* ticks left on the pause between stages */
    int    game_over;     /* ticks left on the game over message */

    /* A fighter freed by shooting its captor, on its way down to dock. */
    bool   rescue_active;
    Vec2   rescue_pos;

    /* What the last bonus round paid, held so the between-stage pause can
       show it. */
    int    bonus_hits;
    int    bonus_award;

    /* Set when the game-over message has finished. The game does not restart
       itself: it says it is done and lets whatever is driving it decide, which
       is how the title screen gets a look in. */
    bool   finished;

    /* Set while a game is being built rather than begun, which happens once
       behind the title screen. Everything works as normal; it just does not
       announce itself. */
    bool   quiet;

    /* The game playing itself on the title screen. It plays for real - the
       same update, the same wave, the same collisions - but it must not be
       able to reach the high score, which would let a machine nobody is
       sitting at beat the person who last played it. */
    bool   demo;

    /* Two-player alternating play. Each seat is a whole Game of its own -
       its own score, stage, crew and wave in progress - because that is what
       taking turns means here: player two's formation is not player one's
       formation with a different score on it.

       `turn_over` is raised when a fighter is lost and lowered by whoever is
       running the session, which is what freezes a game between turns: the
       respawn cannot finish while it is set, so the board waits exactly where
       it was until this seat comes round again. */
    bool   turn_over;

    /* The crew is gone and the message has been shown, but the results screen
       has not run yet. Between the two the board still has to be put away, and
       only whoever is running the session knows when that has happened - so
       the game stops here and waits to be told. Getting this wrong showed as
       one player's results appearing in the middle of the other player's
       turn. */
    bool   over;
    int    seat;          /* 0 or 1, for the banner and the HUD    */
    int    other_score;   /* the other seat's, or -1 when alone    */

    /* Debug aid: stops the fighter dying, so a long --at run reaches the tick
       it was asked for instead of restarting halfway. */
    bool   invulnerable;

    /* Debug aid: reports the state of each new wave as it is handed out, which
       is the direct way to check a stage really does start clean. */
    bool   trace;

    /* Ticks between consecutive deaths. Respawning on top of something still
       flying showed up as a gap of almost nothing, so this is the number that
       says the recall and the spawn check are working. */
    int    last_death_tick;
    int    min_death_gap;

    /* When the fighter was last lost, whether to a hit or to a beam. The wait
       for a replacement is a thing players feel, so it is a thing to measure:
       a capture that leaves the screen empty for ten seconds is a bug however
       correct each individual rule along the way is. */
    int    gone_tick;
} Game;

/* The starfield. Exposed because the title screen flies the same sky, and two
   independent star systems drifting at different rates would be obvious the
   moment the game started. */
void game_background_update(void);
void game_background_draw(Gfx *gfx);

void game_init(Game *g);
void game_restart(Game *g);           /* back to stage 1 with a full crew */
void game_update(Game *g, const Input *in);

/* True once a lost fighter has finished exploding and nothing is left flying.
   A turn cannot be handed over before this: switching boards mid-explosion
   leaves the wreck to appear over the next player's screen. */
bool game_turn_settled(const Game *g);

/* Starts the results screen for a game whose crew is gone. Called once the
   board has been packed away, so the numbers land on an empty sky rather than
   over a formation that is still standing there. */
void game_show_results(Game *g);

/* Makes the music match the stage this game is sitting on, whatever was
   playing before. Idempotent, and safe to call from anywhere that changes
   which game or which stage is on screen. Returns whether the stage wanted
   music, which is the decision a headless run can check - it opens no device,
   so the mixer has nothing to report. */
bool game_stage_music(const Game *g);
void game_draw(Gfx *gfx, const Game *g);

#endif /* CLAUDAGA_GAME_H */
