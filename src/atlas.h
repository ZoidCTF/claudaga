#ifndef GALAGA_ATLAS_H
#define GALAGA_ATLAS_H

#include <SDL.h>
#include "common.h"

/* Every sprite group on the sheet. A "group" is one actor and all of its
   frames: rotation steps for the flyers, animation steps for the explosions
   and the tractor beam, or a set of related icons for the flags and scores. */
typedef enum {
    /* --- The fighter. 7 frames, 15 degrees apart. --- */
    SPR_FIGHTER,           /* the player                                     */
    SPR_FIGHTER_CAPTURED,  /* the red one a Boss Galaga has taken            */

    /* --- Formation enemies. 8 frames each: seven rotation steps like the
       fighter, plus frame 7, a second north-facing pose. Frames 6 and 7 are
       the wing-flap pair the enemies cycle while sitting in formation. --- */
    SPR_BOSS_GREEN,        /* Boss Galaga, teal/yellow                       */
    SPR_BOSS_BLUE,         /* Boss Galaga, blue/magenta. The two entries are  */
                           /* its damaged and undamaged states - it takes two */
                           /* hits and swaps palette in between.              */
    SPR_BUTTERFLY,         /* Goei, red and white                            */
    SPR_BEE,               /* Zako, blue and yellow                          */

    /* --- Challenging-stage flyers. The sheet holds sixteen of these and
       gives no names, so they are numbered by where they sit; see the table
       in atlas.c for the row and column each one came from. --- */
    SPR_BONUS_1,  SPR_BONUS_2,  SPR_BONUS_3,  SPR_BONUS_4,
    SPR_BONUS_5,  SPR_BONUS_6,  SPR_BONUS_7,  SPR_BONUS_8,
    SPR_BONUS_9,  SPR_BONUS_10, SPR_BONUS_11, SPR_BONUS_12,
    SPR_BONUS_13, SPR_BONUS_14, SPR_BONUS_15, SPR_BONUS_16,

    /* --- Effects --- */
    SPR_PLAYER_EXPLOSION,  /* 4 frames, 32x32                                */
    SPR_ENEMY_EXPLOSION,   /* 5 frames, 32x32                                */
    SPR_TRACTOR_BEAM,      /* 3 frames, 48x80                                */

    /* --- Shots --- */
    SPR_PLAYER_MISSILE,    /* 1 frame                                        */
    SPR_ENEMY_MISSILE,     /* 8 frames, one per heading; see MissileDir      */

    /* --- HUD --- */
    SPR_SCORE_VALUE,       /* 8 frames: 150 400 800 1000 1500 1600 2000 3000 */
    SPR_STAGE_FLAG,        /* 6 frames: 1 5 10 20 30 50                      */
    SPR_LIFE_ICON,         /* 1 frame                                        */

    SPR_COUNT
} SpriteId;

/* Frame order of SPR_ENEMY_MISSILE. The sheet lays these out as a 3x3 rose
   whose cells point the way the shot travels, so the names are the sheet's
   own geometry rather than an invention. */
typedef enum {
    MISSILE_N, MISSILE_NE, MISSILE_E, MISSILE_SE,
    MISSILE_S, MISSILE_SW, MISSILE_W, MISSILE_NW,
    MISSILE_DIR_COUNT
} MissileDir;

/* Frame order of SPR_SCORE_VALUE, matching the values Galaga awards for a
   Boss Galaga escort kill. */
typedef enum {
    SCORE_150, SCORE_400, SCORE_800, SCORE_1000,
    SCORE_1500, SCORE_1600, SCORE_2000, SCORE_3000,
    SCORE_VALUE_COUNT
} ScoreValue;

/* Frame order of SPR_STAGE_FLAG. */
typedef enum {
    FLAG_1, FLAG_5, FLAG_10, FLAG_20, FLAG_30, FLAG_50,
    FLAG_COUNT
} StageFlag;

#define ATLAS_MAX_FRAMES 16

typedef struct {
    const char *name;                    /* for debug overlays              */
    SDL_Rect    frame[ATLAS_MAX_FRAMES]; /* source rects into the sheet     */
    int         count;
    int         rot_frames;              /* leading frames that are rotations */
} Sprite;

/* Which frame to draw, and how to mirror it, for a given heading. */
typedef struct {
    int              frame;
    SDL_RendererFlip flip;
} SpritePose;

/* Fills in the sprite table. Cheap, no allocation, safe to call more than
   once. Must run before any of the accessors below. */
void atlas_init(void);

const Sprite    *atlas_get(SpriteId id);
const SDL_Rect  *atlas_frame(SpriteId id, int frame); /* wraps out-of-range */
int              atlas_count(SpriteId id);
const char      *atlas_name(SpriteId id);

/* Every flyer stores the same thing: seven frames turning counter-clockwise
   from north (frame 6) to west (frame 0), 15 degrees at a time. That is one
   quadrant; mirroring it horizontally, vertically, or both covers the other
   three, which is exactly the trick the arcade hardware used. Returns the
   frame and mirroring for any heading. Sprites that do not rotate always come
   back as frame 0 unflipped. */
SpritePose atlas_pose(SpriteId id, float heading_deg);

/* Frames 6 and 7 are both north-facing; enemies alternate them to flap their
   wings while parked in formation. `phase` picks between them and may be any
   free-running counter. Sprites with only rotation frames ignore it. */
int atlas_idle_frame(SpriteId id, int phase);

/* Picks the enemy-missile frame for a velocity, snapping to the nearest of
   the eight headings the sheet provides. */
MissileDir atlas_missile_dir(float vx, float vy);

#endif /* GALAGA_ATLAS_H */
