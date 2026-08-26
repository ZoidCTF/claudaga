#ifndef CLAUDAGA_FX_H
#define CLAUDAGA_FX_H

#include "gfx.h"
#include "shape.h"

/* Short-lived effects: the two explosions and the score popped up where a boss
 * died. Generated rather than drawn - shards thrown outward from the kill read
 * better than fixed frames and scale with the rest of the picture. Each blast
 * carries a seed, so two deaths in one spot differ. */

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
