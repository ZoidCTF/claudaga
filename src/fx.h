#ifndef CLAUDAGA_FX_H
#define CLAUDAGA_FX_H

#include "gfx.h"
#include "atlas.h"

/* Short-lived visual effects: the two explosion animations, and the score
 * value that pops up where a Boss Galaga died. None of them affect play, so
 * they live in a fixed pool that simply overwrites its oldest entry when full
 * rather than doing anything clever. */

#define MAX_FX 24

typedef enum {
    FX_NONE,
    FX_ENEMY_BLAST,    /* 5 frames, 32x32 */
    FX_PLAYER_BLAST,   /* 4 frames, 32x32 */
    FX_SCORE           /* the value sprite, held still then removed */
} FxKind;

typedef struct {
    FxKind kind;
    Vec2   pos;
    int    age;        /* ticks since it started */
    int    life;       /* ticks it lasts */
    int    frames;     /* animation frames, 1 for a score popup */
    int    value;      /* ScoreValue, for FX_SCORE */
} FxItem;

typedef struct {
    FxItem item[MAX_FX];
} Fx;

void fx_reset(Fx *fx);
void fx_blast_enemy(Fx *fx, Vec2 at);
void fx_blast_player(Fx *fx, Vec2 at);
void fx_score(Fx *fx, Vec2 at, ScoreValue value);
void fx_update(Fx *fx);
void fx_draw(Gfx *g, const Fx *fx);

/* True while a player explosion is still playing, which is what holds the
   respawn back until the death has finished being shown. */
bool fx_player_blast_active(const Fx *fx);

#endif /* CLAUDAGA_FX_H */
