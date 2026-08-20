#ifndef CLAUDAGA_FX_H
#define CLAUDAGA_FX_H

#include "gfx.h"
#include "shape.h"

/* Short-lived visual effects: the two explosions, and the score that pops up
 * where a Boss Galaga died.
 *
 * The explosions are generated rather than drawn. As raster art they were four
 * and five fixed frames; as geometry it costs less and reads better to throw
 * shards outward from the kill and let them fade, and it scales with the rest
 * of the picture for free. Each blast carries a seed, so two deaths in the same
 * spot do not produce the same pattern. */

#define MAX_FX 24

typedef enum {
    FX_NONE,
    FX_ENEMY_BLAST,
    FX_PLAYER_BLAST,
    FX_SCORE
} FxKind;

typedef struct {
    FxKind kind;
    Vec2   pos;
    int    age;
    int    life;
    u32    seed;
    int    value;   /* points, for FX_SCORE */
} FxItem;

typedef struct {
    FxItem item[MAX_FX];
    u32    rng;
} Fx;

void fx_reset(Fx *fx);
void fx_blast_enemy(Fx *fx, Vec2 at);
void fx_blast_player(Fx *fx, Vec2 at);
void fx_score(Fx *fx, Vec2 at, int points);
void fx_update(Fx *fx);
void fx_draw(Gfx *g, const Fx *fx);

/* True while a player explosion is still playing, which is what holds the
   respawn back until the death has finished being shown. */
bool fx_player_blast_active(const Fx *fx);

#endif /* CLAUDAGA_FX_H */
