/* The artwork. Every shape faces north on a grid running -8..+8, so it fills
 * the same 16x16 the sprite cells did and the collision radii still hold.
 *
 * Only the right half is written out; `mirror` draws a polygon again flipped
 * across x = 0, which makes the symmetry exact rather than something to get
 * right twice. Polygons paint in the order listed, so wings and shells first.
 *
 * Loosely after the arcade designs rather than traced from them. */

#include "shape.h"

/* ---------------------------------------------------------------- fighter */

static const Vec2 FIGHTER_V[] = {
    /* body      */ {  0.0f, -8.0f }, {  1.5f, -3.0f }, {  1.5f,  4.0f }, {  0.0f,  4.5f },
    /* wing      */ {  1.5f,  1.0f }, {  7.5f,  4.0f }, {  7.5f,  6.0f }, {  1.5f,  4.5f },
    /* wing tip  */ {  6.2f,  3.6f }, {  7.8f,  4.4f }, {  7.8f,  6.4f }, {  6.2f,  5.6f },
    /* engine    */ {  0.5f,  4.5f }, {  1.7f,  4.5f }, {  1.5f,  7.2f }, {  0.7f,  7.2f },
    /* cockpit   */ {  0.0f, -2.2f }, {  1.0f, -1.0f }, {  1.0f,  1.4f }, {  0.0f,  2.2f },
};

static const ShapePoly FIGHTER_P[] = {
    {  4, 4, 0, true },   /* wing behind the hull */
    {  0, 4, 0, true },   /* hull                 */
    {  8, 4, 1, true },   /* red wing tip         */
    { 12, 4, 1, true },   /* engine               */
    { 16, 4, 2, true },   /* cockpit              */
};

static const ShapePalette FIGHTER_PAL = {{
    { 232, 232, 255, 255 },   /* hull, white with a lavender cast */
    { 255,  48,  48, 255 },   /* red trim                         */
    {  72, 148, 255, 255 },   /* cockpit                          */
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

/* The fighter a Boss Galaga has taken: same ship, enemy colours. */
const ShapePalette SHAPE_PAL_FIGHTER_CAPTURED = {{
    { 255,  64,  64, 255 },
    { 232, 232, 255, 255 },
    { 255, 200,  64, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

/* -------------------------------------------------------------------- bee */

static const Vec2 BEE_V[] = {
    /* wing      */ {  1.5f, -2.5f }, {  7.8f,  1.0f }, {  7.2f,  4.2f }, {  1.5f,  1.0f },
    /* antenna   */ {  0.6f, -7.5f }, {  1.8f, -5.5f }, {  0.9f, -4.6f },
    /* head      */ {  0.0f, -5.2f }, {  2.2f, -3.6f }, {  2.2f, -1.0f }, {  0.0f, -0.6f },
    /* thorax    */ {  0.0f, -1.0f }, {  2.4f, -0.6f }, {  2.4f,  3.0f }, {  0.0f,  3.4f },
    /* band      */ {  0.0f,  0.4f }, {  2.4f,  0.2f }, {  2.4f,  1.9f }, {  0.0f,  2.1f },
    /* abdomen   */ {  0.0f,  3.0f }, {  2.0f,  3.0f }, {  1.6f,  6.0f }, {  0.0f,  6.4f },
};

static const ShapePoly BEE_P[] = {
    {  0, 4, 0, true },   /* blue wing, behind everything */
    {  4, 3, 1, true },   /* antenna                      */
    {  7, 4, 1, true },   /* head                         */
    { 11, 4, 1, true },   /* thorax                       */
    { 15, 4, 2, true },   /* red band                     */
    { 19, 4, 0, true },   /* abdomen                      */
};

static const ShapePalette BEE_PAL = {{
    {  40,  96, 240, 255 },   /* blue   */
    { 255, 216,   0, 255 },   /* yellow */
    { 255,  48,  48, 255 },   /* red    */
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

/* -------------------------------------------------------------- butterfly */

static const Vec2 BUTTERFLY_V[] = {
    /* upper wing */ {  1.6f, -4.5f }, {  8.0f, -6.5f }, {  7.4f, -1.2f }, {  1.8f, -0.2f },
    /* lower wing */ {  1.8f,  0.2f }, {  7.6f,  0.8f }, {  6.6f,  5.8f }, {  1.8f,  3.6f },
    /* body       */ {  0.0f, -6.0f }, {  1.8f, -4.5f }, {  1.8f,  4.0f }, {  0.0f,  5.0f },
    /* band       */ {  0.0f, -1.4f }, {  1.8f, -1.4f }, {  1.8f,  1.8f }, {  0.0f,  1.8f },
    /* head       */ {  0.0f, -6.0f }, {  1.2f, -5.2f }, {  1.2f, -3.4f }, {  0.0f, -3.4f },
};

static const ShapePoly BUTTERFLY_P[] = {
    {  0, 4, 0, true },
    {  4, 4, 0, true },
    {  8, 4, 1, true },
    { 12, 4, 2, true },
    { 16, 4, 2, true },
};

static const ShapePalette BUTTERFLY_PAL = {{
    { 240,  40,  40, 255 },   /* red wings */
    { 240, 240, 255, 255 },   /* white body */
    {  72, 148, 255, 255 },   /* blue core  */
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

/* ------------------------------------------------------------------- boss */

static const Vec2 BOSS_V[] = {
    /* horn        */ {  1.8f, -8.0f }, {  3.4f, -6.8f }, {  2.8f, -4.6f }, {  1.6f, -5.6f },
    /* dome        */ {  0.0f, -6.0f }, {  3.2f, -5.0f }, {  4.4f, -1.8f }, {  0.0f, -1.4f },
    /* upper claw  */ {  4.0f, -3.0f }, {  7.8f, -1.4f }, {  7.4f,  1.0f }, {  4.2f, -0.4f },
    /* lower claw  */ {  5.2f,  0.6f }, {  7.6f,  1.4f }, {  6.2f,  5.0f }, {  4.4f,  3.6f },
    /* lower shell */ {  0.0f,  1.0f }, {  3.4f,  1.6f }, {  2.8f,  5.4f }, {  0.0f,  6.0f },
    /* core        */ {  0.0f, -3.6f }, {  2.8f, -2.6f }, {  2.8f,  1.6f }, {  0.0f,  2.4f },
    /* foot        */ {  0.8f,  3.4f }, {  2.4f,  3.0f }, {  2.2f,  6.6f }, {  1.0f,  6.8f },
};

static const ShapePoly BOSS_P[] = {
    {  0, 4, 2, true },   /* horn                                   */
    {  4, 4, 0, true },   /* dome                                   */
    {  8, 4, 0, true },   /* claws, held clear of the shell so they */
    { 12, 4, 0, true },   /* read separately at a small size        */
    { 16, 4, 0, true },   /* lower shell                            */
    { 20, 4, 1, true },   /* core                                   */
    { 24, 4, 2, true },   /* foot                                   */
};

static const ShapePalette BOSS_PAL = {{
    {   0, 168, 176, 255 },   /* teal shell */
    { 255, 232,   0, 255 },   /* core       */
    { 240, 100,   0, 255 },   /* orange     */
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

/* A boss that has already taken one hit. Same shell, different colours - on
   the sprite sheet this needed a second full set of frames. */
const ShapePalette SHAPE_PAL_BOSS_HIT = {{
    {  96,  96, 240, 255 },
    { 255, 128, 255, 255 },
    { 176,  48, 224, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

/* ------------------------------------------------------------------ shots */

static const Vec2 PLAYER_SHOT_V[] = {
    /* outer */ {  0.0f, -6.0f }, {  1.6f, -1.5f }, {  1.6f,  2.5f }, {  0.0f,  4.5f },
    /* inner */ {  0.0f, -4.5f }, {  0.8f, -1.0f }, {  0.8f,  1.8f }, {  0.0f,  3.0f },
};

static const ShapePoly PLAYER_SHOT_P[] = {
    { 0, 4, 1, true },   /* cyan body      */
    { 4, 4, 0, true },   /* white hot core */
};

static const ShapePalette PLAYER_SHOT_PAL = {{
    { 255, 255, 255, 255 },
    { 120, 220, 255, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

static const Vec2 ENEMY_SHOT_V[] = {
    /* head */ {  0.0f, -4.2f }, {  2.0f, -1.0f }, {  0.0f,  1.8f },
    /* spot */ {  0.0f, -2.4f }, {  0.9f, -0.8f }, {  0.0f,  0.6f },
    /* tail */ {  0.0f,  1.2f }, {  1.2f,  2.0f }, {  0.8f,  5.0f }, {  0.0f,  5.2f },
};

static const ShapePoly ENEMY_SHOT_P[] = {
    { 6, 4, 1, true },   /* red tail behind the head */
    { 0, 3, 0, true },
    { 3, 3, 2, true },
};

static const ShapePalette ENEMY_SHOT_PAL = {{
    {  80, 150, 255, 255 },
    { 255,  60,  60, 255 },
    { 240, 240, 255, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

/* ------------------------------------------------- challenging-stage flyers */

/* These never form up and never shoot; they fly a pattern through the screen
   and are worth points if caught on the way. Being free of the formation, they
   can be shaped more loosely than the regulars. */

static const Vec2 MOTH_V[] = {
    /* upper wing */ {  1.4f, -3.0f }, {  7.8f, -6.8f }, {  7.6f, -1.6f }, {  1.8f, -0.4f },
    /* lower wing */ {  1.8f,  0.2f }, {  7.2f,  1.6f }, {  6.2f,  6.2f }, {  1.8f,  3.4f },
    /* wing spot  */ {  3.4f, -4.4f }, {  6.0f, -5.4f }, {  5.8f, -2.8f }, {  3.4f, -2.2f },
    /* body       */ {  0.0f, -5.6f }, {  1.6f, -4.2f }, {  1.6f,  4.6f }, {  0.0f,  5.6f },
    /* head       */ {  0.0f, -5.6f }, {  1.1f, -4.8f }, {  1.1f, -3.0f }, {  0.0f, -3.0f },
};

static const ShapePoly MOTH_P[] = {
    {  0, 4, 0, true }, {  4, 4, 0, true }, {  8, 4, 2, true },
    { 12, 4, 1, true }, { 16, 4, 2, true },
};

static const ShapePalette MOTH_PAL = {{
    {  60, 200, 120, 255 }, { 230, 255, 230, 255 }, { 255, 160,  40, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

static const Vec2 SCORPION_V[] = {
    /* pincer */ {  2.0f, -7.4f }, {  5.4f, -6.2f }, {  5.0f, -3.6f }, {  2.2f, -4.6f },
    /* leg    */ {  3.0f, -1.4f }, {  7.4f,  1.0f }, {  7.0f,  3.0f }, {  3.0f,  1.0f },
    /* body   */ {  0.0f, -4.6f }, {  3.0f, -3.4f }, {  3.0f,  2.2f }, {  0.0f,  3.2f },
    /* tail   */ {  0.0f,  2.8f }, {  2.2f,  3.0f }, {  1.6f,  7.2f }, {  0.0f,  7.4f },
    /* eye    */ {  0.0f, -3.4f }, {  1.5f, -2.6f }, {  1.5f, -1.0f }, {  0.0f, -0.6f },
};

static const ShapePoly SCORPION_P[] = {
    {  0, 4, 0, true }, {  4, 4, 0, true }, {  8, 4, 0, true },
    { 12, 4, 1, true }, { 16, 4, 2, true },
};

static const ShapePalette SCORPION_PAL = {{
    { 150,  70, 220, 255 }, { 255, 220,  60, 255 }, { 240, 240, 255, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

static const Vec2 DART_V[] = {
    /* fin    */ {  1.8f,  0.0f }, {  6.8f,  4.4f }, {  6.4f,  6.4f }, {  1.8f,  4.0f },
    /* body   */ {  0.0f, -8.0f }, {  1.8f, -2.0f }, {  1.8f,  4.0f }, {  0.0f,  5.2f },
    /* stripe */ {  0.0f, -3.0f }, {  1.0f, -1.6f }, {  1.0f,  2.0f }, {  0.0f,  3.0f },
    /* tip    */ {  0.0f, -8.0f }, {  1.2f, -4.4f }, {  0.0f, -4.0f },
};

static const ShapePoly DART_P[] = {
    {  0, 4, 0, true }, {  4, 4, 0, true }, {  8, 4, 1, true }, { 12, 3, 2, true },
};

static const ShapePalette DART_PAL = {{
    { 230,  50,  90, 255 }, { 255, 240, 240, 255 }, { 255, 210,  60, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

static const Vec2 ORB_V[] = {
    /* upper spoke */ {  3.0f, -3.6f }, {  7.6f, -5.0f }, {  7.6f, -2.4f }, {  3.2f, -1.2f },
    /* lower spoke */ {  3.2f,  1.2f }, {  7.6f,  2.4f }, {  7.6f,  5.0f }, {  3.0f,  3.6f },
    /* core        */ {  0.0f, -5.0f }, {  3.6f, -3.2f }, {  3.6f,  3.2f }, {  0.0f,  5.0f },
    /* eye         */ {  0.0f, -2.0f }, {  1.8f, -1.0f }, {  1.8f,  1.0f }, {  0.0f,  2.0f },
};

static const ShapePoly ORB_P[] = {
    {  0, 4, 1, true }, {  4, 4, 1, true }, {  8, 4, 0, true }, { 12, 4, 2, true },
};

static const ShapePalette ORB_PAL = {{
    { 230,  60, 200, 255 }, {  80, 230, 240, 255 }, { 255, 255, 255, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

/* ------------------------------------------------------------------ flags */

/* A shield: broad shoulders, a banded middle, tapering to a point. Small, so
   it is built to read at about ten pixels across rather than to be looked at. */
static const Vec2 FLAG_V[] = {
    /* shield */ {  0.0f, -7.0f }, {  4.6f, -5.6f }, {  4.6f,  2.2f }, {  0.0f,  7.2f },
    /* crest  */ {  0.0f, -5.4f }, {  2.0f, -4.6f }, {  2.0f, -2.8f }, {  0.0f, -2.2f },
    /* band   */ {  0.0f, -2.0f }, {  4.4f, -1.6f }, {  4.4f,  0.8f }, {  0.0f,  1.2f },
    /* point  */ {  0.0f,  3.0f }, {  3.2f,  2.4f }, {  1.9f,  6.4f }, {  0.0f,  6.9f },
};

static const ShapePoly FLAG_P[] = {
    {  0, 4, 0, true },   /* shield */
    {  8, 4, 1, true },   /* band   */
    { 12, 4, 1, true },   /* point  */
    {  4, 4, 2, true },   /* crest  */
};

/* The default is the one-flag; the rest are handed in by the caller. */
static const ShapePalette FLAG_PAL_1 = {{
    { 232,  60,  60, 255 }, { 245, 245, 245, 255 }, { 245, 245, 245, 255 },
    {   0,   0,   0, 255 }, {   0,   0,   0, 255 }, {   0,   0,   0, 255 },
}};

const int SHAPE_FLAG_VALUE[FLAG_KINDS] = { 50, 30, 20, 10, 5, 1 };

const ShapePalette SHAPE_PAL_FLAG[FLAG_KINDS] = {
    /* 50 */ {{ { 245, 245, 250, 255 }, { 224,  48,  48, 255 }, { 224,  48,  48, 255 },
                {0,0,0,255},{0,0,0,255},{0,0,0,255} }},
    /* 30 */ {{ {  64, 104, 232, 255 }, { 255, 210,  40, 255 }, { 245, 245, 250, 255 },
                {0,0,0,255},{0,0,0,255},{0,0,0,255} }},
    /* 20 */ {{ { 200,  56, 200, 255 }, { 255, 210,  40, 255 }, { 255, 210,  40, 255 },
                {0,0,0,255},{0,0,0,255},{0,0,0,255} }},
    /* 10 */ {{ {  40, 160, 176, 255 }, { 245, 245, 250, 255 }, { 255, 210,  40, 255 },
                {0,0,0,255},{0,0,0,255},{0,0,0,255} }},
    /*  5 */ {{ { 240, 140,  40, 255 }, { 245, 245, 250, 255 }, { 240, 140,  40, 255 },
                {0,0,0,255},{0,0,0,255},{0,0,0,255} }},
    /*  1 */ {{ { 232,  60,  60, 255 }, { 245, 245, 245, 255 }, { 245, 245, 245, 255 },
                {0,0,0,255},{0,0,0,255},{0,0,0,255} }},
};

/* ------------------------------------------------------------------ table */

static const Shape SHAPES[SHP_COUNT] = {
    { "FIGHTER",     FIGHTER_V,     FIGHTER_P,     (int)ARRAY_COUNT(FIGHTER_P),     &FIGHTER_PAL     },
    { "BEE",         BEE_V,         BEE_P,         (int)ARRAY_COUNT(BEE_P),         &BEE_PAL         },
    { "BUTTERFLY",   BUTTERFLY_V,   BUTTERFLY_P,   (int)ARRAY_COUNT(BUTTERFLY_P),   &BUTTERFLY_PAL   },
    { "BOSS",        BOSS_V,        BOSS_P,        (int)ARRAY_COUNT(BOSS_P),        &BOSS_PAL        },
    { "PLAYER SHOT", PLAYER_SHOT_V, PLAYER_SHOT_P, (int)ARRAY_COUNT(PLAYER_SHOT_P), &PLAYER_SHOT_PAL },
    { "ENEMY SHOT",  ENEMY_SHOT_V,  ENEMY_SHOT_P,  (int)ARRAY_COUNT(ENEMY_SHOT_P),  &ENEMY_SHOT_PAL  },
    { "MOTH",        MOTH_V,        MOTH_P,        (int)ARRAY_COUNT(MOTH_P),        &MOTH_PAL        },
    { "SCORPION",    SCORPION_V,    SCORPION_P,    (int)ARRAY_COUNT(SCORPION_P),    &SCORPION_PAL    },
    { "DART",        DART_V,        DART_P,        (int)ARRAY_COUNT(DART_P),        &DART_PAL        },
    { "ORB",         ORB_V,         ORB_P,         (int)ARRAY_COUNT(ORB_P),         &ORB_PAL         },
    { "FLAG",        FLAG_V,        FLAG_P,        (int)ARRAY_COUNT(FLAG_P),        &FLAG_PAL_1      },
};

const Shape *shape_get(ShapeId id)
{
    if (id < 0 || id >= SHP_COUNT) return NULL;
    return &SHAPES[id];
}

const char *shape_name(ShapeId id)
{
    const Shape *s = shape_get(id);
    return s ? s->name : "?";
}
