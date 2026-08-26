#ifndef CLAUDAGA_SHAPE_H
#define CLAUDAGA_SHAPE_H

#include "gfx.h"

/* Vector artwork, drawn as coloured triangles through SDL_RenderGeometry.
 *
 * Replaces the sprite sheet for everything that flies, which buys resolution
 * independence, continuous rotation, and free recolouring - a damaged boss and
 * the captured fighter are palette swaps rather than drawings.
 *
 * One unit is one pixel of the 224x288 picture, the origin is the sprite's
 * centre, and the artwork faces north, so sizes match the 16x16 cells the
 * collision radii were tuned against.
 *
 * Every polygon is a triangle fan, so each must be convex: a concave wing is
 * two convex pieces, and there is no triangulator to get wrong. */

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
