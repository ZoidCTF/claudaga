#ifndef CLAUDAGA_GAME_H
#define CLAUDAGA_GAME_H

#include "gfx.h"
#include "formation.h"
#include "fx.h"

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
    int    stage;
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
} Game;

/* The starfield. Exposed because the title screen flies the same sky, and two
   independent star systems drifting at different rates would be obvious the
   moment the game started. */
void game_background_update(void);
void game_background_draw(Gfx *gfx);

void game_init(Game *g);
void game_restart(Game *g);           /* back to stage 1 with a full crew */
void game_update(Game *g, const Uint8 *keys);
void game_draw(Gfx *gfx, const Game *g);

#endif /* CLAUDAGA_GAME_H */
