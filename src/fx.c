#include "fx.h"
#include "debugfont.h"

#include <math.h>
#include <stdio.h>

#define ENEMY_BLAST_LIFE   26
#define PLAYER_BLAST_LIFE  42
#define SCORE_LIFE         90

#define ENEMY_SHARDS   11
#define PLAYER_SHARDS  15

void fx_reset(Fx *fx)
{
    for (int i = 0; i < MAX_FX; ++i) fx->item[i].kind = FX_NONE;
    fx->rng = 0xC1A0DA6Au;
}

static u32 rng_next(Fx *fx)
{
    u32 x = fx->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    fx->rng = x;
    return x;
}

/* Free slot, or the oldest one if the pool is full. Dropping the oldest is the
   right trade for effects: the newest explosion is the one being looked at. */
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

static void spawn_blast(Fx *fx, Vec2 at, FxKind kind, int life)
{
    FxItem *it = claim(fx);
    it->kind  = kind;
    it->pos   = at;
    it->age   = 0;
    it->life  = life;
    it->seed  = rng_next(fx);
    it->value = 0;
}

void fx_blast_enemy(Fx *fx, Vec2 at)
{
    spawn_blast(fx, at, FX_ENEMY_BLAST, ENEMY_BLAST_LIFE);
}

void fx_blast_player(Fx *fx, Vec2 at)
{
    spawn_blast(fx, at, FX_PLAYER_BLAST, PLAYER_BLAST_LIFE);
}

void fx_score(Fx *fx, Vec2 at, int points)
{
    FxItem *it = claim(fx);
    it->kind  = FX_SCORE;
    it->pos   = at;
    it->age   = 0;
    it->life  = SCORE_LIFE;
    it->seed  = 0;
    it->value = points;
}

void fx_update(Fx *fx)
{
    for (int i = 0; i < MAX_FX; ++i) {
        FxItem *it = &fx->item[i];
        if (it->kind == FX_NONE) continue;
        if (++it->age >= it->life) it->kind = FX_NONE;
    }
}

/* One shard: a wedge pointing away from the centre, `d` out along `ang` and
   `len` long. Drawn as a triangle, which is all a fleck of debris needs. */
static void shard(Gfx *g, Vec2 centre, float ang, float d, float len,
                  float wide, SDL_Color c)
{
    float sx = sinf(ang), sy = -cosf(ang);
    float px = sy,        py = -sx;          /* perpendicular */

    Vec2 tri[3];
    tri[0].x = centre.x + sx * (d + len);
    tri[0].y = centre.y + sy * (d + len);
    tri[1].x = centre.x + sx * d + px * wide;
    tri[1].y = centre.y + sy * d + py * wide;
    tri[2].x = centre.x + sx * d - px * wide;
    tri[2].y = centre.y + sy * d - py * wide;
    shape_draw_poly(g, tri, 3, c);
}

static SDL_Color mix(SDL_Color a, SDL_Color b, float t)
{
    SDL_Color r;
    r.r = (Uint8)(a.r + (b.r - a.r) * t);
    r.g = (Uint8)(a.g + (b.g - a.g) * t);
    r.b = (Uint8)(a.b + (b.b - a.b) * t);
    r.a = (Uint8)(a.a + (b.a - a.a) * t);
    return r;
}

static void draw_blast(Gfx *g, const FxItem *it, int shards, float reach,
                       SDL_Color hot, SDL_Color cool)
{
    float t = (float)it->age / (float)it->life;      /* 0..1 */
    float ease = 1.0f - (1.0f - t) * (1.0f - t);     /* fast out, then coast */

    SDL_Color c = mix(hot, cool, t);
    c.a = (Uint8)(255.0f * (1.0f - t * t));

    /* The seed only jitters the angles, so every blast is the same shape of
       event without being the same picture. */
    u32 s = it->seed | 1u;
    for (int i = 0; i < shards; ++i) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        float jitter = ((float)(s & 0xFFFFu) / 65535.0f - 0.5f) * 0.45f;
        float ang    = (float)i / (float)shards * 2.0f * (float)M_PI + jitter;

        float d    = ease * reach;
        float len  = (1.0f - t) * reach * 0.45f + 1.0f;
        float wide = (1.0f - t) * 2.2f + 0.4f;
        shard(g, it->pos, ang, d, len, wide, c);
    }

    /* A core that collapses as the shards leave, so the middle does not simply
       empty out on the first frame. */
    if (t < 0.55f) {
        float r = (1.0f - t / 0.55f) * reach * 0.42f;
        SDL_Color core = hot;
        core.a = (Uint8)(255.0f * (1.0f - t / 0.55f));
        Vec2 quad[4] = {
            { it->pos.x,     it->pos.y - r },
            { it->pos.x + r, it->pos.y     },
            { it->pos.x,     it->pos.y + r },
            { it->pos.x - r, it->pos.y     },
        };
        shape_draw_poly(g, quad, 4, core);
    }
}

void fx_draw(Gfx *g, const Fx *fx)
{
    static const SDL_Color ENEMY_HOT  = { 255, 240, 140, 255 };
    static const SDL_Color ENEMY_COOL = { 235,  60,  30, 255 };
    static const SDL_Color PLAY_HOT   = { 235, 255, 255, 255 };
    static const SDL_Color PLAY_COOL  = {  40, 190, 255, 255 };
    static const SDL_Color SCORE_COL  = { 255, 235, 120, 255 };

    for (int i = 0; i < MAX_FX; ++i) {
        const FxItem *it = &fx->item[i];
        switch (it->kind) {
        case FX_NONE:
            break;

        case FX_ENEMY_BLAST:
            draw_blast(g, it, ENEMY_SHARDS, 15.0f, ENEMY_HOT, ENEMY_COOL);
            break;

        case FX_PLAYER_BLAST:
            draw_blast(g, it, PLAYER_SHARDS, 21.0f, PLAY_HOT, PLAY_COOL);
            break;

        case FX_SCORE: {
            /* The value is drawn rather than blitted now. The sheet's score
               sprites only covered the boss tiers; text covers any number and
               drops the last reason the play view needed the sheet. */
            char buf[16];
            snprintf(buf, sizeof buf, "%d", it->value);
            SDL_Color c = SCORE_COL;
            float t = (float)it->age / (float)it->life;
            c.a = (Uint8)(255.0f * (t > 0.75f ? (1.0f - t) * 4.0f : 1.0f));
            font_draw(g, (int)it->pos.x - font_width(buf) / 2,
                         (int)it->pos.y - FONT_H / 2, c, buf);
            break;
        }
        }
    }
}

bool fx_player_blast_active(const Fx *fx)
{
    for (int i = 0; i < MAX_FX; ++i) {
        if (fx->item[i].kind == FX_PLAYER_BLAST) return true;
    }
    return false;
}
