#ifndef GALAGA_GAME_H
#define GALAGA_GAME_H

#include "gfx.h"
#include "formation.h"
#include "fx.h"

/* The playable scene: the starfield, the fighter and its shots, the attack
 * wave, and everything that happens when those things touch. It owns the
 * collision checks because it is the only thing that can see both sides. */

/* One player missile in the air at a time. Missing therefore costs real time
   - the shot has to clear the top of the screen before another can be fired -
   which is where most of the difficulty in this game comes from. (The arcade
   allowed two; one is a deliberate choice here.) */
#define MAX_SHOTS   2
#define START_LIVES 3

typedef struct {
    Vec2 pos;
    bool alive;
} Shot;

typedef struct {
    float x;
    bool  alive;
    int   lives;
    int   respawn;     /* ticks left before it comes back */
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

    /* Debug aid: stops the fighter dying, so a long --at run reaches the tick
       it was asked for instead of restarting halfway. */
    bool   invulnerable;

    /* Debug aid: reports the state of each new wave as it is handed out, which
       is the direct way to check a stage really does start clean. */
    bool   trace;
} Game;

void game_init(Game *g);
void game_restart(Game *g);           /* back to stage 1 with a full crew */
void game_update(Game *g, const Uint8 *keys);
void game_draw(Gfx *gfx, const Game *g);

#endif /* GALAGA_GAME_H */
