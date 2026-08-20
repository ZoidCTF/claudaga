#ifndef CLAUDAGA_SHAPE_H
#define CLAUDAGA_SHAPE_H

#include "gfx.h"

/* Vector artwork, drawn as coloured triangles through SDL_RenderGeometry.
 *
 * This replaces the sprite sheet for everything that flies. Three things fall
 * out of it that the raster art could not give:
 *
 *   - it is resolution independent, so the game can render at the window's
 *     real size instead of an integer multiple of 224x288;
 *   - rotation is continuous, which retires the whole business of storing one
 *     quadrant of frames and mirroring it;
 *   - recolouring is free, so a damaged Boss Galaga and the captured red
 *     fighter are palette swaps rather than separate drawings.
 *
 * Shapes are authored in the same units the game already thinks in: one unit
 * is one pixel of the 224x288 picture, the origin is the centre of the sprite,
 * and the artwork faces north. Sizes therefore match the 16x16 cells the
 * collision radii were tuned against, so nothing about gameplay shifts.
 *
 * Every polygon is drawn as a triangle fan, which means each one must be
 * convex. That is a real constraint on the artwork rather than a limitation
 * worth engineering around: a concave wing is two convex pieces, and keeping
 * the renderer a fan means no triangulator to write or get wrong. */

#define SHAPE_PAL_MAX 6

typedef struct {
    SDL_Color c[SHAPE_PAL_MAX];
} ShapePalette;

typedef struct {
    unsigned char first;   /* first vertex of the fan, into the shape's list */
    unsigned char count;   /* how many vertices the fan uses                 */
    unsigned char pal;     /* which palette entry fills it                   */
    bool          mirror;  /* also draw it flipped across x = 0              */
} ShapePoly;

typedef struct {
    const char         *name;
    const Vec2         *vert;
    const ShapePoly    *poly;
    int                 poly_count;
    const ShapePalette *palette;
} Shape;

typedef enum {
    SHP_FIGHTER,
    SHP_BEE,
    SHP_BUTTERFLY,
    SHP_BOSS,
    SHP_PLAYER_SHOT,
    SHP_ENEMY_SHOT,

    /* Challenging-stage flyers. The arcade fields a different set each bonus
       round; these four cycle. */
    SHP_MOTH,
    SHP_SCORPION,
    SHP_DART,
    SHP_ORB,

    SHP_FLAG,   /* one shield, six palettes: the stage-count flags */

    SHP_COUNT
} ShapeId;

/* The bonus flyers in order, for a challenging stage to pick from. */
#define SHP_BONUS_FIRST SHP_MOTH
#define SHP_BONUS_COUNT 4

const Shape *shape_get(ShapeId id);
const char  *shape_name(ShapeId id);

/* Alternative palettes: the captured fighter, and a Boss Galaga that has
   already taken a hit. Both are the same artwork in different colours. */
extern const ShapePalette SHAPE_PAL_FIGHTER_CAPTURED;
extern const ShapePalette SHAPE_PAL_BOSS_HIT;

/* The stage flags. One shield drawn six ways: on the arcade cabinet these are
   six separate pieces of art, but the whole job of a flag is to be told apart
   from the others at a glance, which colour alone does. Values run 1, 5, 10,
   20, 30, 50 and a stage count is spelled out with the largest first. */
#define FLAG_KINDS 6
extern const int          SHAPE_FLAG_VALUE[FLAG_KINDS];
extern const ShapePalette SHAPE_PAL_FLAG[FLAG_KINDS];

/* Draws `id` centred on `pos`, turned `heading` degrees clockwise from north.
   `scale` of 1 draws it at its authored size. */
void shape_draw(Gfx *g, ShapeId id, Vec2 pos, float heading, float scale);

/* As above with the palette replaced and a uniform alpha applied, which is
   what the explosions and the recoloured actors use. */
void shape_draw_pal(Gfx *g, ShapeId id, Vec2 pos, float heading, float scale,
                    const ShapePalette *pal, float alpha);

/* A filled convex polygon straight from world-space points. The procedural
   effects build their geometry per frame and have no fixed artwork, so they
   go through this rather than the shape table. */
void shape_draw_poly(Gfx *g, const Vec2 *pts, int n, SDL_Color color);

#endif /* CLAUDAGA_SHAPE_H */
