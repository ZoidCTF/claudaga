#include "atlas.h"

#include <math.h>   /* M_PI arrives via SDL_stdinc.h */

/* The sheet keeps its 16x16 art on a regular grid: a 1px border, then cells
   every 18px (16 of art, 2 of gutter). These turn a column/row into pixels. */
#define GX(col) (1 + 18 * (col))
#define GY(row) (1 + 18 * (row))

static Sprite s_sprites[SPR_COUNT];
static bool   s_ready = false;

/* Groups whose frames are evenly spaced along a row - which is nearly all of
   them. `pitch` is the x step from one frame to the next. */
typedef struct {
    SpriteId    id;
    const char *name;
    int         x, y, w, h, pitch, count;
} Run;

static const Run RUNS[] = {
    /* id                    name                    x        y        w   h  pitch cnt */
    { SPR_FIGHTER,           "FIGHTER",              GX(0),   GY(0),  16, 16, 18,  7 },
    { SPR_FIGHTER_CAPTURED,  "FIGHTER CAPTURED",     GX(0),   GY(1),  16, 16, 18,  7 },

    { SPR_BOSS_GREEN,        "BOSS GALAGA GREEN",    GX(0),   GY(2),  16, 16, 18,  8 },
    { SPR_BOSS_BLUE,         "BOSS GALAGA BLUE",     GX(0),   GY(3),  16, 16, 18,  8 },
    { SPR_BUTTERFLY,         "BUTTERFLY",            GX(0),   GY(4),  16, 16, 18,  8 },
    { SPR_BEE,               "BEE",                  GX(0),   GY(5),  16, 16, 18,  8 },

    /* Challenging-stage flyers, left half of the sheet (rows 6-11). */
    { SPR_BONUS_1,           "BONUS 1",              GX(0),   GY(6),  16, 16, 18,  7 },
    { SPR_BONUS_2,           "BONUS 2",              GX(0),   GY(7),  16, 16, 18,  7 },
    { SPR_BONUS_3,           "BONUS 3",              GX(0),   GY(8),  16, 16, 18,  7 },
    { SPR_BONUS_4,           "BONUS 4",              GX(0),   GY(9),  16, 16, 18,  7 },
    { SPR_BONUS_5,           "BONUS 5",              GX(0),   GY(10), 16, 16, 18,  6 },
    { SPR_BONUS_6,           "BONUS 6",              GX(0),   GY(11), 16, 16, 18,  7 },
    /* ...and the right half (rows 2-11, starting at column 8). */
    { SPR_BONUS_7,           "BONUS 7",              GX(8),   GY(2),  16, 16, 18,  8 },
    { SPR_BONUS_8,           "BONUS 8",              GX(8),   GY(3),  16, 16, 18,  8 },
    { SPR_BONUS_9,           "BONUS 9",              GX(8),   GY(4),  16, 16, 18,  8 },
    { SPR_BONUS_10,          "BONUS 10",             GX(8),   GY(5),  16, 16, 18,  7 },
    { SPR_BONUS_11,          "BONUS 11",             GX(8),   GY(6),  16, 16, 18,  7 },
    { SPR_BONUS_12,          "BONUS 12",             GX(8),   GY(7),  16, 16, 18,  7 },
    { SPR_BONUS_13,          "BONUS 13",             GX(8),   GY(8),  16, 16, 18,  7 },
    { SPR_BONUS_14,          "BONUS 14",             GX(8),   GY(9),  16, 16, 18,  7 },
    { SPR_BONUS_15,          "BONUS 15",             GX(8),   GY(10), 16, 16, 18,  6 },
    { SPR_BONUS_16,          "BONUS 16",             GX(8),   GY(11), 16, 16, 18,  6 },

    /* The big art sits above and to the right of the 16x16 grid and keeps its
       own spacing: 34px for the 32x32 blasts, 50px for the beam. */
    { SPR_PLAYER_EXPLOSION,  "PLAYER EXPLOSION",     145,     1,      32, 32, 34,  4 },
    { SPR_ENEMY_EXPLOSION,   "ENEMY EXPLOSION",      289,     1,      32, 32, 34,  5 },
    { SPR_TRACTOR_BEAM,      "TRACTOR BEAM",         289,     36,     48, 80, 50,  3 },
};

/* Cells of the 3x3 missile rose, indexed by MissileDir. The whole 16x16 cell
   is used rather than a tight crop: each shot's art is already positioned
   inside its cell to lean the right way, so drawing the cell centred on the
   projectile puts the head and tail where they belong. */
static const SDL_Rect MISSILE_CELL[MISSILE_DIR_COUNT] = {
    { 307, 118, 16, 16 },  /* N  */
    { 325, 118, 16, 16 },  /* NE */
    { 325, 136, 16, 16 },  /* E  */
    { 325, 154, 16, 16 },  /* SE */
    { 307, 154, 16, 16 },  /* S  */
    { 289, 154, 16, 16 },  /* SW */
    { 289, 136, 16, 16 },  /* W  */
    { 289, 118, 16, 16 },  /* NW */
};

/* The centre of the rose, which is the player's shot rather than a ninth
   heading. Taken as a full cell so it lines up with the enemy missiles. */
static const SDL_Rect PLAYER_MISSILE_CELL = { 307, 136, 16, 16 };

/* The HUD art is not on any grid, so these are the measured ink bounds -
   tight, because the game centres them on a target rather than tiling them. */
static const SDL_Rect SCORE_RECT[SCORE_VALUE_COUNT] = {
    { 344, 122, 14, 7 },  /*  150 */
    { 361, 122, 15, 7 },  /*  400 */
    { 379, 122, 15, 7 },  /*  800 */
    { 397, 122, 16, 7 },  /* 1000 */
    { 415, 122, 16, 7 },  /* 1500 */
    { 343, 140, 16, 7 },  /* 1600 */
    { 368, 140, 20, 7 },  /* 2000 */
    { 402, 140, 20, 7 },  /* 3000 */
};

static const SDL_Rect FLAG_RECT[FLAG_COUNT] = {
    { 307, 176,  7, 12 },  /*  1 */
    { 317, 174,  7, 14 },  /*  5 */
    { 328, 174, 13, 14 },  /* 10 */
    { 345, 172, 15, 16 },  /* 20 */
    { 363, 172, 15, 16 },  /* 30 */
    { 381, 172, 15, 16 },  /* 50 */
};

static const SDL_Rect LIFE_ICON_RECT = { 290, 173, 13, 14 };

/* The flyers - the fighter, the formation enemies, and the challenging-stage
   ones - are the sprites drawn at an angle. Everything after them in the enum
   is an effect, a shot, or HUD art and is always drawn upright. */
static bool is_flyer(SpriteId id)
{
    return id >= SPR_FIGHTER && id <= SPR_BONUS_16;
}

static void set_list(SpriteId id, const char *name, const SDL_Rect *rects, int n)
{
    Sprite *s = &s_sprites[id];
    s->name  = name;
    s->count = n;
    for (int i = 0; i < n && i < ATLAS_MAX_FRAMES; ++i) s->frame[i] = rects[i];
}

void atlas_init(void)
{
    if (s_ready) return;

    for (int i = 0; i < ARRAY_COUNT(RUNS); ++i) {
        const Run *r = &RUNS[i];
        Sprite *s = &s_sprites[r->id];
        s->name  = r->name;
        s->count = r->count;
        for (int f = 0; f < r->count && f < ATLAS_MAX_FRAMES; ++f) {
            s->frame[f].x = r->x + r->pitch * f;
            s->frame[f].y = r->y;
            s->frame[f].w = r->w;
            s->frame[f].h = r->h;
        }
        /* Seven rotation steps where there are seven to be had. The handful of
           six-frame challenging-stage rows are treated as a shorter sweep over
           the same quadrant; they are not used in a stage yet, so that reading
           is untested. */
        s->rot_frames = is_flyer(r->id) ? (r->count >= 7 ? 7 : r->count) : 0;
    }

    set_list(SPR_PLAYER_MISSILE, "PLAYER MISSILE", &PLAYER_MISSILE_CELL, 1);
    set_list(SPR_ENEMY_MISSILE,  "ENEMY MISSILE",  MISSILE_CELL, MISSILE_DIR_COUNT);
    set_list(SPR_SCORE_VALUE,    "SCORE VALUES",   SCORE_RECT,   SCORE_VALUE_COUNT);
    set_list(SPR_STAGE_FLAG,     "STAGE FLAGS",    FLAG_RECT,    FLAG_COUNT);
    set_list(SPR_LIFE_ICON,      "LIFE ICON",      &LIFE_ICON_RECT, 1);

    s_ready = true;
}

const Sprite *atlas_get(SpriteId id)
{
    SDL_assert(id >= 0 && id < SPR_COUNT);
    return &s_sprites[id];
}

const SDL_Rect *atlas_frame(SpriteId id, int frame)
{
    const Sprite *s = atlas_get(id);
    if (s->count <= 0) return &s->frame[0];
    /* Wrap rather than clamp so callers can hand us a free-running tick. */
    frame %= s->count;
    if (frame < 0) frame += s->count;
    return &s->frame[frame];
}

int atlas_count(SpriteId id) { return atlas_get(id)->count; }

const char *atlas_name(SpriteId id)
{
    const char *n = atlas_get(id)->name;
    return n ? n : "?";
}
