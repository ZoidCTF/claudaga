#include "fx.h"

/* Ticks each animation frame is held for. The arcade blasts are quick; slower
   than this and a kill stops feeling like a hit. */
#define BLAST_FRAME_TICKS 5
#define SCORE_LIFE        90

void fx_reset(Fx *fx)
{
    for (int i = 0; i < MAX_FX; ++i) fx->item[i].kind = FX_NONE;
}

/* Free slot, or the oldest one if the pool is full. Dropping the oldest is the
   right trade for effects: the newest explosion is the one the player is
   looking at. */
static FxItem *claim(Fx *fx)
{
    FxItem *oldest = &fx->item[0];
    for (int i = 0; i < MAX_FX; ++i) {
        FxItem *it = &fx->item[i];
        if (it->kind == FX_NONE) return it;
        if (it->age > oldest->age) oldest = it;
    }
    return oldest;
}

static void spawn_blast(Fx *fx, Vec2 at, FxKind kind, SpriteId sprite)
{
    FxItem *it = claim(fx);
    it->kind   = kind;
    it->pos    = at;
    it->age    = 0;
    it->frames = atlas_count(sprite);
    it->life   = it->frames * BLAST_FRAME_TICKS;
    it->value  = 0;
}

void fx_blast_enemy(Fx *fx, Vec2 at)
{
    spawn_blast(fx, at, FX_ENEMY_BLAST, SPR_ENEMY_EXPLOSION);
}

void fx_blast_player(Fx *fx, Vec2 at)
{
    spawn_blast(fx, at, FX_PLAYER_BLAST, SPR_PLAYER_EXPLOSION);
}

void fx_score(Fx *fx, Vec2 at, ScoreValue value)
{
    FxItem *it = claim(fx);
    it->kind   = FX_SCORE;
    it->pos    = at;
    it->age    = 0;
    it->frames = 1;
    it->life   = SCORE_LIFE;
    it->value  = (int)value;
}

void fx_update(Fx *fx)
{
    for (int i = 0; i < MAX_FX; ++i) {
        FxItem *it = &fx->item[i];
        if (it->kind == FX_NONE) continue;
        if (++it->age >= it->life) it->kind = FX_NONE;
    }
}

void fx_draw(Gfx *g, const Fx *fx)
{
    for (int i = 0; i < MAX_FX; ++i) {
        const FxItem *it = &fx->item[i];
        if (it->kind == FX_NONE) continue;

        const SDL_Rect *src;
        if (it->kind == FX_SCORE) {
            src = atlas_frame(SPR_SCORE_VALUE, it->value);
        } else {
            SpriteId id = (it->kind == FX_PLAYER_BLAST) ? SPR_PLAYER_EXPLOSION
                                                        : SPR_ENEMY_EXPLOSION;
            int frame = it->age / BLAST_FRAME_TICKS;
            if (frame >= it->frames) frame = it->frames - 1;
            src = atlas_frame(id, frame);
        }
        gfx_blit(g, src, (int)(it->pos.x - src->w / 2.0f),
                         (int)(it->pos.y - src->h / 2.0f));
    }
}

bool fx_player_blast_active(const Fx *fx)
{
    for (int i = 0; i < MAX_FX; ++i) {
        if (fx->item[i].kind == FX_PLAYER_BLAST) return true;
    }
    return false;
}
