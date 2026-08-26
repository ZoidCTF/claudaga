#include "formation.h"
#include "audio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ slots */

/* Four bosses across the top, two ranks of eight butterflies, two of ten bees.
   Flights take slots eight at a time in this order, as on the real board. */
typedef struct {
    ShapeId shape;
    int     first_col, count;
} SlotRank;

static const SlotRank SLOT_RANKS[FORM_ROWS] = {
    { SHP_BOSS,      3, 4  },
    { SHP_BUTTERFLY, 1, 8  },
    { SHP_BUTTERFLY, 1, 8  },
    { SHP_BEE,       0, 10 },
    { SHP_BEE,       0, 10 },
};

typedef struct {
    ShapeId shape;
    int     col, row;
} Slot;

static Slot s_slots[MAX_ENEMIES];
static int  s_slot_count;

static void build_slots(void)
{
    s_slot_count = 0;
    for (int r = 0; r < FORM_ROWS; ++r) {
        const SlotRank *rank = &SLOT_RANKS[r];
        for (int i = 0; i < rank->count && s_slot_count < MAX_ENEMIES; ++i) {
            Slot *sl = &s_slots[s_slot_count++];
            sl->shape  = rank->shape;
            sl->col    = rank->first_col + i;
            sl->row    = r;
        }
    }
}

Vec2 formation_slot_pos(int slot)
{
    Vec2 v = { GAME_W * 0.5f, FORM_Y };
    if (slot < 0 || slot >= s_slot_count) return v;
    v.x = (float)(FORM_X + s_slots[slot].col * FORM_PITCH);
    v.y = (float)(FORM_Y + s_slots[slot].row * FORM_PITCH);
    return v;
}

/* ------------------------------------------------------------------ paths */

/* Control points are screen pixels and the curve passes through every one.
   Paths start off-screen so an enemy is up to speed before it is visible. */

/* Down the middle, then a full loop low on the screen - the signature Galaga
   move. Ends below the formation heading north, where the per-slot join curve
   takes over; arriving from beneath is what stops that join hooking back. */
static const Vec2 CTRL_TOP_DIVE[] = {
    { 100, -28 }, { 100,  56 }, { 100, 132 }, { 100, 190 },
    {  88, 226 }, {  60, 240 }, {  32, 226 }, {  22, 196 },
    {  34, 168 }, {  62, 158 }, {  94, 150 },
};

/* In from the left, climbing into a loop across the top, ending below the
   formation like the dive. Occupies a different band of the screen so two
   flights at once stay legible. Enters above the fighter's row and never comes
   back down to it: an enemy arriving from off-screen at the fighter's own
   altitude cannot be dodged. */
static const Vec2 CTRL_SWEEP[] = {
    { -28, 196 }, {   4, 182 }, {  32, 162 }, {  50, 136 },
    {  56, 110 }, {  72,  88 }, { 102,  74 }, { 134,  88 },
    { 144, 118 }, { 122, 142 }, {  96, 134 },
};

/* The way home. Divers leave at the bottom, so they re-enter over the top,
   drop down outside the parked block, and turn back up beneath it - arriving in
   the same state as a new enemy, so the same join curve finishes the trip. */
static const Vec2 CTRL_RETURN[] = {
    {  16, -24 }, {  18,  40 }, {  22, 104 }, {  36, 148 },
    {  68, 166 }, {  96, 156 },
};

/* Bonus-round passes: an S down the screen, and a climb out through the top.
   These begin and end off-screen - nothing forms up in a bonus round. */
static const Vec2 CTRL_CHAL_A[] = {
    {  40, -30 }, {  70,  26 }, {  38,  76 }, {  56, 130 },
    { 118, 160 }, { 176, 194 }, { 150, 250 }, {  96, 300 },
    {  60, 340 },
};

static const Vec2 CTRL_CHAL_B[] = {
    {  24, 320 }, {  40, 250 }, {  92, 214 }, { 146, 186 },
    { 160, 136 }, { 124, 106 }, {  78, 116 }, {  66,  70 },
    {  96,  16 }, { 132, -34 },
};

/* A lateral crossing: on from one side, a shallow serpentine straight across
   the middle of the screen, off the other. The only pass that does not spend
   most of its time travelling down the screen, which is what makes it read
   differently from the rest - the flyers stay at a constant height and it is
   the horizontal lead that has to be judged. */
static const Vec2 CTRL_CHAL_C[] = {
    { -34, 110 }, {  26, 150 }, {  74, 112 }, { 126, 156 },
    { 172, 118 }, { 214, 162 }, { 258, 124 },
};

/* A corkscrew: in at the top, a curl to one side, a cut back across into a
   wider swing the other way, then out of the bottom. Two turns in opposite
   directions mean a flyer on this pass crosses its own column twice, so a
   shot led into the first turn can be waited out for the second. */
/* The later rounds' shapes, and the point of them: they stay in a column.
 *
 * A, B, C and D all cross the whole screen, which is what made every bonus
 * round beatable from the middle - whatever a lane does, it does some of it
 * overhead, so anywhere is as good as anywhere. These two sweep about fifty
 * pixels, so where a flyer can be shot is a place rather than everywhere, and
 * a round built from several of them at different columns has to be learnt.
 *
 * Authored around the middle of the screen and moved sideways per lane, which
 * is also why the later rounds no longer use mirrors: reflecting a pair puts it
 * back either side of centre, and a fighter parked between them covers both.
 * Translating does not. */

/* Down the screen, one tight loop, and out of the bottom. */
static const Vec2 CTRL_CHAL_E[] = {
    { 112, -30 }, { 112,  26 }, { 110,  74 }, { 114, 118 },
    { 132, 148 }, { 126, 178 }, {  98, 184 }, {  86, 158 },
    { 100, 134 }, { 124, 140 }, { 130, 176 }, { 120, 224 },
    { 112, 276 }, { 108, 340 },
};

/* Down, a hook across the low band, and back out of the top - two passes over
   the same column for a fighter that knows to be under it. */
static const Vec2 CTRL_CHAL_F[] = {
    { 112, -30 }, { 118,  36 }, { 128,  92 }, { 126, 140 },
    { 108, 172 }, {  84, 158 }, {  78, 122 }, {  92,  92 },
    { 104,  54 }, { 100,  -8 }, {  96, -40 },
};

static const Vec2 CTRL_CHAL_D[] = {
    { 112, -30 }, {  96,  24 }, {  56,  52 }, {  46,  96 },
    {  86, 116 }, { 120,  92 }, { 146, 124 }, { 174, 166 },
    { 138, 196 }, { 104, 226 }, { 118, 276 }, { 132, 330 },
};

/* A long diagonal from one top corner to the opposite low edge, a loop there,
   and back up the middle. It crosses the whole screen rather than working one
   half of it, which is what makes it read differently from the dive and the
   sweep when it turns up among them. Like the others it ends below the
   formation heading north, so the same join finishes the trip. */
static const Vec2 CTRL_CORNER[] = {
    { -26, -20 }, {  30,  40 }, {  84,  92 }, { 140, 140 },
    { 178, 186 }, { 176, 226 }, { 140, 240 }, { 108, 220 },
    { 100, 184 }, {  96, 148 },
};

/* --------------------------------------------------------------- flights */

/* Eight enemies per flight, alternating between two mirrored paths so the
   stream splits and enters from both sides at once. */
#define FLIGHT_SIZE  8
#define FLIGHT_COUNT (MAX_ENEMIES / FLIGHT_SIZE)

typedef struct {
    PathId path_a, path_b;
    int    start_tick;
    int    spacing;      /* ticks between one enemy and the next */
} Flight;

/* Four entry sets of five flights each, cycled by ordinary stage. Cadence
   varies as much as the curves do. Set 0 is the original, so stage one always
   opens the same way. */
#define ENTRY_SETS 4

static const Flight ENTRIES[ENTRY_SETS][FLIGHT_COUNT] = {
    {   /* the original: dives and sweeps, alternating sides */
        { PATH_TOP_DIVE_L, PATH_TOP_DIVE_R,   0, 11 },
        { PATH_SWEEP_L,    PATH_SWEEP_R,    130, 11 },
        { PATH_TOP_DIVE_R, PATH_TOP_DIVE_L, 260, 11 },
        { PATH_SWEEP_R,    PATH_SWEEP_L,    390, 11 },
        { PATH_SWEEP_L,    PATH_SWEEP_R,    520, 11 },
    },
    {   /* corners across the middle, tighter and quicker */
        { PATH_CORNER_L,   PATH_CORNER_R,     0, 10 },
        { PATH_SWEEP_R,    PATH_SWEEP_L,    120, 10 },
        { PATH_CORNER_R,   PATH_CORNER_L,   240, 10 },
        { PATH_TOP_DIVE_L, PATH_TOP_DIVE_R, 360, 10 },
        { PATH_CORNER_L,   PATH_CORNER_R,   480,  9 },
    },
    {   /* down the outside first, then across - slower and wider */
        { PATH_RETURN_L,   PATH_RETURN_R,     0, 12 },
        { PATH_TOP_DIVE_R, PATH_TOP_DIVE_L, 140, 12 },
        { PATH_CORNER_L,   PATH_CORNER_R,   280, 11 },
        { PATH_RETURN_R,   PATH_RETURN_L,   420, 12 },
        { PATH_SWEEP_L,    PATH_SWEEP_R,    550, 11 },
    },
    {   /* everything at once: the shortest gaps of the four */
        { PATH_SWEEP_L,    PATH_SWEEP_R,      0,  9 },
        { PATH_CORNER_R,   PATH_CORNER_L,   110,  9 },
        { PATH_TOP_DIVE_L, PATH_TOP_DIVE_R, 220,  9 },
        { PATH_RETURN_L,   PATH_RETURN_R,   330, 10 },
        { PATH_CORNER_L,   PATH_CORNER_R,   440,  9 },
    },
};

#define ENTRY_SPEED  2.6f    /* pixels per tick along an entry or return path */

/* The difficulty ramp. Each pair is stage 1 and the far end; stages between
   interpolate, past RAMP_STAGES sits at the hard end.
 *
 * The cap is a bound on the burst, not a driver - it is never actually reached,
 * because a dive lasts ~150 ticks and the burst below is what fills the screen.
 * Entry speed does not ramp: the entry is watched, not played. */
#define RAMP_STAGES  12

#define ATTACK_INTERVAL      105    /* ticks between attacks, stage 1  */
#define ATTACK_INTERVAL_END   44

#define DIVE_SPEED_1        3.1f    /* pixels per tick down a dive     */
#define DIVE_SPEED_END      4.3f

#define DIVER_CAP_1            3    /* dive groups in the air at once  */
#define DIVER_CAP_END  MAX_DIVERS

#define SWAY_PERIOD_1     420.0f    /* ticks for one full sway cycle   */
#define SWAY_PERIOD_END   240.0f

/* Bursts change the shape of the pressure rather than the rate: two or three
   attacks together, then a gap.
 *
 * The gap is measured, not guessed - at 16 the worst pair convoys for 27 ticks,
 * at 24 for 9. Past about 28 a burst outlasts the interval after it, which is
 * an even stream with extra steps. */
#define BURST_LEN_END   3
#define BURST_GAP      24

/* How close two dive groups must be, and for how long, to count as travelling
   together rather than crossing. Only the distance is enforced; the tick count
   is the line the reported figure is read against. */
#define CONVOY_DIST  14.0f
#define CONVOY_TICKS 20

/* How much faster the last bonus round flies than the first. Small - a bonus
   stage is a shooting gallery, not a threat. */
#define CHAL_SPEED_RAMP 0.30f

/* A flat trim on every bonus round; they were authored too fast to read. Kept
   out of the per-round table so that still says how the rounds differ. */

/* Flyers per group. Eight groups of five fill the wave. */
#define CHAL_GROUP 5
#define CHAL_SPEED_TRIM 0.75f

/* How far the parked block drifts either side of its slots. Unlike everything
   above this does not ramp: the outer columns sit 40px from the screen edge
   and a 16px sprite needs 8 of that, so the amplitude is bounded by the screen
   rather than by taste. Later stages sway faster instead of wider. */
#define SWAY_AMP 10.0f

/* What a bonus round pays lives in the header, since the game shows the
   totals: CHALLENGE_HIT_SCORE per flyer caught, CHALLENGE_PERFECT for catching
   every one. The perfect bonus is worth far more than the sum of the hits,
   which is what makes a challenging stage a thing to be good at rather than a
   free forty shots. */

/* Minimum gap between two enemies entering the same return lane. At entry
   speed this works out around 50px of daylight, comfortable for a 16px
   sprite. */
#define RETURN_SPACING 20

/* Where a boss's escorts ride: this far along the path ahead of it, and this
   far to either side. The boss sits at the trailing apex of the triangle.
   Both have to clear a 16px sprite or the trio reads as one smeared blob -
   the lead is what separates the escorts from the boss, the spread what
   separates them from each other. */
#define ESCORT_LEAD   21.0f
#define ESCORT_SPREAD 17.0f

/* Missiles. Divers shoot on the way down; a parked enemy never does.

   Aim is not a straight line to the fighter. It can only move along its row,
   so a missile arriving flat along that row cannot be dodged. Two rules: the
   shooter must be some way above, and the aim is clamped into a cone about
   straight down. */
#define ENEMY_SHOT_SPEED  2.3f
/* How much less often an enemy flying in shoots than one attacking. An entry
   puts forty enemies on screen in eleven seconds, so the dive rate would be a
   wall of fire. Nothing shoots on the way in during stage one. */
#define ENTRY_FIRE_RATIO 5

#define FIRE_CHANCE_IN    150     /* per diving enemy, per tick, stage 1 */
#define FIRE_CHANCE_IN_END 68     /* and at the far end of the ramp      */
/* The least warning a missile may give. Written as time and the height derived
   from it: 44px sounds like clearance but is 19 ticks, about a person's
   reaction time with nothing left for moving afterwards. */
#define FIRE_MIN_WARNING  28.0f
#define FIRE_MIN_HEIGHT   (ENEMY_SHOT_SPEED * FIRE_MIN_WARNING)
#define FIRE_MAX_SLOPE    1.0f    /* |dx| <= dy: 45 degrees from straight down */

static bool is_boss(ShapeId s);

/* ------------------------------------------------------------ the beam */

/* The capture routine, in ticks. A boss drops out of formation, hangs there
   with the beam open long enough to be worth dodging or shooting, then shuts
   it and goes home. */
#define BEAM_DESCEND  56
#define BEAM_OPEN     18
#define BEAM_HOLD     96
#define BEAM_CLOSE    18
#define BEAM_TOTAL    (BEAM_DESCEND + BEAM_OPEN + BEAM_HOLD + BEAM_CLOSE)

/* The cone itself. It hangs below the boss and widens as it falls. */
#define BEAM_LEN    122.0f
#define BEAM_TOP_HW   5.0f
#define BEAM_BOT_HW  26.0f
#define BEAM_BANDS   16

/* How far down the boss hangs while the beam is open. Chosen with BEAM_LEN:
   hover plus length must reach PLAYER_Y or nothing can ever be caught. */
#define BEAM_HOVER_Y 146

/* How far the beam has opened, 0 to 1. */
static float beam_open(const Enemy *e)
{
    int t = e->beam_t;
    if (t < BEAM_DESCEND) return 0.0f;
    t -= BEAM_DESCEND;
    if (t < BEAM_OPEN)    return (float)t / (float)BEAM_OPEN;
    t -= BEAM_OPEN;
    if (t < BEAM_HOLD)    return 1.0f;
    t -= BEAM_HOLD;
    return 1.0f - (float)t / (float)BEAM_CLOSE;
}

/* Half-width of the cone at `depth` below its mouth. */
static float beam_half_width(float depth, float open)
{
    float t = depth / (BEAM_LEN * open);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (BEAM_TOP_HW + (BEAM_BOT_HW - BEAM_TOP_HW) * t) * open;
}

bool wave_beam_catch(const Wave *w, Vec2 at, int *boss)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (e->state != ENEMY_BEAMING || e->has_captive) continue;

        float open = beam_open(e);
        if (open <= 0.05f) continue;

        float depth = at.y - e->pos.y;
        if (depth < 0.0f || depth > BEAM_LEN * open) continue;
        if (fabsf(at.x - e->pos.x) > beam_half_width(depth, open)) continue;

        if (boss) *boss = i;
        return true;
    }
    return false;
}

void wave_attach_captive(Wave *w, int boss)
{
    if (boss < 0 || boss >= MAX_ENEMIES) return;
    w->enemies[boss].has_captive = true;
    w->captive_holder = boss;
}

Vec2 wave_enemy_pos(const Wave *w, int index)
{
    Vec2 v = { GAME_W * 0.5f, FORM_Y };
    if (index < 0 || index >= MAX_ENEMIES) return v;
    return w->enemies[index].pos;
}

int wave_captive_holder(const Wave *w)
{
    return w->captive_holder;
}

/* A captured fighter rides directly above its captor in screen space, always -
 * parked, entering or diving. Screen space rather than rotated with the boss is
 * what makes it read as "above" rather than "behind the nose".
 *
 * It also decides who can be shot. Screen y grows downward, so a shot climbing
 * from the fighter's row meets the boss first: the boss shields the captive,
 * and a straight shot up the column frees it rather than destroying it. */
#define CAPTIVE_ABOVE 15.0f

static Vec2 captive_offset(const Enemy *e)
{
    (void)e;
    Vec2 o = { 0.0f, -CAPTIVE_ABOVE };
    return o;
}

Vec2 wave_captive_pos(const Wave *w)
{
    Vec2 v = { GAME_W * 0.5f, FORM_Y };
    int h = w->captive_holder;
    if (h < 0 || h >= MAX_ENEMIES) return v;

    const Enemy *e = &w->enemies[h];
    Vec2 o = captive_offset(e);
    v.x = e->pos.x + o.x;
    v.y = e->pos.y + o.y;
    return v;
}

bool wave_captive_hit(Wave *w, Vec2 at)
{
    int h = w->captive_holder;
    if (h < 0 || h >= MAX_ENEMIES) return false;
    if (!w->enemies[h].has_captive) return false;

    Vec2 c = wave_captive_pos(w);
    float dx = at.x - c.x, dy = at.y - c.y;
    if (dx * dx + dy * dy > ENEMY_HIT_RADIUS * ENEMY_HIT_RADIUS) return false;

    w->enemies[h].has_captive = false;
    w->captive_holder = -1;
    return true;
}

/* ------------------------------------------------------------------- rng */

/* xorshift32. Small, adequate for choosing attackers, and - unlike rand() % n
   on MSVC's 15-bit generator - it does not lean on low bits of poor quality. */
static u32 rng_next(Wave *w)
{
    u32 x = w->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    w->rng = x;
    return x;
}

/* Uniform in 0..n-1. */
static int rng_below(Wave *w, int n)
{
    return n > 0 ? (int)(rng_next(w) % (u32)n) : 0;
}

/* ------------------------------------------------------------ dive paths */

/* The attack curves. All four are built from the leader's parked position, the
 * side it breaks towards and the fighter's column, and all four end below the
 * screen, which hands the enemy to the return lane. Which one an enemy flies is
 * chosen per attack, not per stage.
 *
 * They leave by deliberately different doors. Four shapes take four different
 * times to reach the tail, so the separation that launch timing gives two
 * groups is gone by then; a shared exit put them on top of each other for half
 * a second. The pass at the fighter is what matters, so the exits are spread. */
typedef enum {
    DIVE_PEEL,     /* the original: out, over, and down across the screen */
    DIVE_LOOP,     /* a tight loop on the way out, then a run at the player */
    DIVE_CROSS,    /* out to one edge, then hard across and down */
    DIVE_PLUNGE,   /* steep, straight and late-hooking */
    DIVE_SHAPES
} DiveShape;

/* Keeps a deliberately wide control point far enough inside the screen that
   the curve round it is still visible. */
static float on_screen(float x)
{
    const float M = 26.0f;
    if (x < M)              return M;
    if (x > GAME_W - M)     return GAME_W - M;
    return x;
}

/* The approach every attack curve makes near the fighter, and the rule all four
 * obey: a dive may come down through the fighter, but may not turn along its row
 * and come at it sideways faster than PLAYER_SPEED, because then there is
 * nowhere to go.
 *
 * A curve lines up AIM_LEAD_UP above its aim, with the width of the screen to do
 * it in. From the aim down it may drift sideways by only AIM_SLOPE of its drop.
 * The four points are spaced so the Catmull-Rom tangents at the aim and at the
 * fighter's row are both exactly AIM_SLOPE - equal tangents on a straight chord
 * give a straight segment, so the stretch crossing the fighter is a line at a
 * known angle rather than whatever a spline decided. `with_lead` is for a curve
 * that arrives lined up under its own steam.
 *
 * Sideways speed across the row, before and after: 4.3 px/tick - the whole dive
 * speed, i.e. dead horizontal - down to 1.5, against the fighter's 1.6. */
/* How far either side of the row counts as level with the fighter. A little
   wider than the radius that kills, so a shave counts. */
#define FIGHTER_BAND 12.0f

#define AIM_LEAD_UP  20.0f    /* how far above the aim a curve lines up   */
#define AIM_Y_MIN    216.0f   /* the highest a curve commits to its line  */
#define AIM_Y_VARY   24       /* and how much lower it may happen instead */
/* Not a free choice. The lining-up leg is the one part of the tail whose
   tangent still points back at wherever the curve came from, so it can be as
   flat as it likes and must finish before the fighter's band starts. */
typedef char aim_lines_up_above_the_band[
    (AIM_Y_MIN + AIM_Y_VARY <= PLAYER_Y - FIGHTER_BAND) ? 1 : -1];
#define AIM_EXIT_Y   340.0f

/* The rule as a multiple of "the fighter can outrun it". At 1.0 an attack can
   never cross the row sideways faster than the fighter moves. Relaxing it to
   2.0 buys about one convoy tick in fifteen, which is not worth the guarantee;
   the knob exists so a difficulty pass can weaken this knowingly. */
#define AIM_TILT     1.00f
#define AIM_SLOPE    (AIM_TILT * PLAYER_SPEED / DIVE_SPEED_END)

/* Where an attack aims, how hard it slants past, and the height it commits at.
 * Drawn per launch to keep attacks apart, because a shared approach is a funnel.
 * Ablated over 576 runs: each is worth about two convoy ticks and none is
 * decisive; they are kept because they are nearly free.
 *
 * Drawn from a pixel-wide range rather than a few fixed lanes - the pairs that
 * convoy are the ones that drew the same lane, so quantising was the mistake.
 * Missing the fighter is correct: an attack that always ends on its exact
 * column is one it has to move for every time. */
#define AIM_SPREAD    20      /* pixels either side of the fighter        */

/* Each curve's aim, offset from the fighter towards the side it broke to.
   Ordered loop, cross, plunge, peel and spaced wider than the convoy distance,
   so two attacks are held apart by construction rather than luck. */
#define AIM_OUTER     26.0f
#define AIM_INNER      9.0f
#define AIM_SLANT_MIN 0.35f   /* share of AIM_SLOPE; 1.0 is the full tilt */
#define AIM_SLANTS    8

typedef struct { float off; float slant; float y; } Aim;

static Aim aim_pick(Wave *w)
{
    Aim a;
    a.off   = (float)(rng_below(w, 2 * AIM_SPREAD + 1) - AIM_SPREAD);
    a.slant = AIM_SLANT_MIN + (1.0f - AIM_SLANT_MIN)
                            * (float)rng_below(w, AIM_SLANTS)
                            / (float)(AIM_SLANTS - 1);
    /* Free to vary: the approach's four points are collinear whatever the
       spacing between them, so the tangents still come out at AIM_SLOPE. */
    a.y     = AIM_Y_MIN + (float)rng_below(w, AIM_Y_VARY + 1);
    return a;
}

/* Clamped less tightly than on_screen: an edge that folded the spread back
   together would undo it. */
static float aim_column(float player_x, float bias, Aim aim)
{
    const float M = 14.0f;
    float x = player_x + bias + aim.off;
    if (x < M)              return M;
    if (x > GAME_W - M)     return GAME_W - M;
    return x;
}

/* Where a curve has to have arrived by, AIM_LEAD_UP above where it aims. */
static float aim_lead_x(float x, float sweep)
{
    return x - sweep * AIM_SLOPE * AIM_LEAD_UP;
}

/* A curve's body runs from the formation down to wherever this attack lines up,
 * and that varies, so the shape scales to fit rather than being clipped to fit.
 * Points are placed as fractions of the room available and sideways excursions
 * scale by the same factor - a body that loses height but keeps its width turns
 * harder in the same arc, which took the cross's sharpest corner from 97 degrees
 * per 12px to 172: a bee that stops dead and reverses. */
#define BODY_GAP   14.0f
#define BODY_SPAN  124.0f   /* the height these shapes were drawn at */

static float body_span(float from_y, float lead_y)
{
    float span = lead_y - BODY_GAP - from_y;
    return span > 40.0f ? span : 40.0f;
}

static float body_scale(float span)
{
    /* The clamps are guards, not tuning: over the formation as it stands the
       factor runs about 0.6 to 1.0 and neither end binds. They are here so that
       moving a row, or the aim, cannot quietly produce a dive squashed to
       nothing. */
    float k = span / BODY_SPAN;
    if (k < 0.55f) k = 0.55f;
    if (k > 1.15f) k = 1.15f;
    return k;
}

static int aim_tail(Vec2 *c, int n, float x, float sweep, float aim_y,
                    int with_lead)
{
    if (with_lead) {
        c[n].x = aim_lead_x(x, sweep);  c[n].y = aim_y - AIM_LEAD_UP;  ++n;
    }
    c[n].x = x;  c[n].y = aim_y;  ++n;
    c[n].x = x + sweep * AIM_SLOPE * ((float)PLAYER_Y - aim_y);
    c[n].y = (float)PLAYER_Y;  ++n;                        /* across the row */
    c[n].x = x + sweep * AIM_SLOPE * (AIM_EXIT_Y - aim_y);
    c[n].y = AIM_EXIT_Y;  ++n;                             /* and gone       */
    return n;
}

static void build_dive_path(Path *out, Vec2 from, int side, float player_x,
                            int shape, Aim aim)
{
    float s     = (float)side;
    float slant = aim.slant;
    float lead  = aim.y - AIM_LEAD_UP;   /* where this one has to be lined up */
    float span  = body_span(from.y, lead);
    float scale = body_scale(span);
    Vec2  c[10];
    int   n = 0;

    switch (shape) {
    default:
    case DIVE_PEEL:
        c[0] = from;
        c[1].x = from.x + s * 13.0f * scale;  c[1].y = from.y - 9.0f * scale;  /* rise out  */
        c[2].x = from.x + s * 35.0f * scale;  c[2].y = from.y + 0.12f * span; /* curl over */
        c[3].x = from.x + s * 30.0f * scale;  c[3].y = from.y + 0.51f * span; /* fall away */
        c[4].x = (from.x + player_x) * 0.5f - s * 22.0f;
        c[4].y = from.y + span;                                  /* swing in  */
        n = aim_tail(c, 5, aim_column(player_x, s * AIM_OUTER, aim), s * slant,
                     aim.y, 1);
        break;

    case DIVE_LOOP: {
        /* A corkscrew: one full turn, descending the whole way through it. A
         * closed circle would have to climb for half its length, so the descent
         * D must be at least 2*pi*R for the vertical speed never to go negative.
         * R is derived from D rather than chosen, which is what stops the shape
         * coming out wrong for an enemy parked high or low.
         *
         * Sized to finish where this dive lines up, with the centre sliding
         * across to the aim column as it goes round - sideways travel spread
         * over a whole revolution is what makes it a corkscrew leaning at the
         * fighter rather than a loop followed by a sprint. */
        float drop = lead - from.y;               /* room before the approach */
        if (drop < 60.0f) drop = 60.0f;
        float R  = drop / 9.0f;                   /* well clear of 2*pi, so it never lifts */
        float x0 = on_screen(from.x + s * 10.0f);
        float x1 = aim_lead_x(aim_column(player_x, -s * AIM_OUTER, aim), s * slant);
        float cy = from.y + R;                    /* the turn starts at `from` */

        /* One turn at fifths. Even spacing matters: control points a few pixels
           apart make the spline stutter through them. */
        static const float SIN5[5] = { 0.951f,  0.588f, -0.588f, -0.951f,  0.0f };
        static const float COS5[5] = { 0.309f, -0.809f, -0.809f,  0.309f,  1.0f };

        c[0] = from;
        for (int k = 0; k < 5; ++k) {
            float t  = (float)(k + 1) / 5.0f;
            float cx = x0 + (x1 - x0) * t;
            c[k + 1].x = cx + s * R * SIN5[k];
            c[k + 1].y = cy + t * drop - R * COS5[k];
        }
        /* The turn already ends lined up, so no lead point. */
        n = aim_tail(c, 6, aim_column(player_x, -s * AIM_OUTER, aim), s * slant,
                     aim.y, 0);
        break;
    }

    case DIVE_CROSS:
        /* Out to the near edge and then hard across the screen, so the enemy
           arrives from the side the player is not watching. */
        c[0] = from;
        c[1].x = from.x + s * 16.0f * scale;          c[1].y = from.y + 0.06f * span;
        c[2].x = on_screen(from.x + s * 52.0f * scale);
        c[2].y = from.y + 0.35f * span;
        /* The turnaround at the edge is the sharpest thing any of these curves
           does; at a wider throw over a squashed span it doubled back through
           130 degrees in twelve pixels. */
        c[3].x = on_screen(GAME_W * 0.5f + s * 74.0f * scale);
        c[3].y = from.y + 0.72f * span;
        /* Carries a share of the launching slot through. Fixed here, every
           cross ever flown passed through the same point and they funnelled -
           nine of the eleven worst convoying pairs. */
        c[4].x = on_screen(GAME_W * 0.5f - s * 40.0f * scale
                           + (from.x - GAME_W * 0.5f) * 0.30f);
        c[4].y = from.y + span;
        n = aim_tail(c, 5, aim_column(player_x, -s * AIM_INNER, aim), s * slant,
                     aim.y, 1);
        break;

    case DIVE_PLUNGE:
        /* Barely a curve at all until the end. It gives the least warning of
           the four, which is why it is the last one a stage unlocks. */
        c[0] = from;
        c[1].x = from.x + s * 7.0f * scale;   c[1].y = from.y + 0.13f * span;
        c[2].x = from.x + s * 2.0f * scale;   c[2].y = from.y + 0.52f * span;
        c[3].x = from.x - s * 10.0f * scale;  c[3].y = from.y + span;
        /* The hook stays on the side the plunge broke towards. Cutting back
           across ran it head-on into the loop's approach, which comes the other
           way; parallel, they pass. */
        n = aim_tail(c, 4, aim_column(player_x, s * AIM_INNER, aim), s * slant,
                     aim.y, 1);
        break;
    }

    path_build(out, c, n);
}

static int dive_path_alloc(Wave *w)
{
    for (int i = 0; i < MAX_DIVERS; ++i) {
        if (w->dive_refs[i] == 0) return i;
    }
    return -1;
}

/* The path returns to the pool when the last of its group is off it. */
static void dive_path_release(Wave *w, int i)
{
    if (i >= 0 && i < MAX_DIVERS && w->dive_refs[i] > 0) --w->dive_refs[i];
}

/* ----------------------------------------------------------------- setup */

static u32  wave_seed_base = 0;
static bool wave_track = false;

void wave_track_challenge(bool on) { wave_track = on; }

void wave_set_seed(unsigned base) { wave_seed_base = (u32)base; }

void wave_init(Wave *w)
{
    build_slots();

    path_build(&w->paths[PATH_TOP_DIVE_L], CTRL_TOP_DIVE, ARRAY_COUNT(CTRL_TOP_DIVE));
    path_build(&w->paths[PATH_SWEEP_L],    CTRL_SWEEP,    ARRAY_COUNT(CTRL_SWEEP));
    path_build(&w->paths[PATH_RETURN_L],   CTRL_RETURN,   ARRAY_COUNT(CTRL_RETURN));
    path_build(&w->paths[PATH_CORNER_L],   CTRL_CORNER,   ARRAY_COUNT(CTRL_CORNER));
    path_build(&w->paths[PATH_CHAL_A_L],   CTRL_CHAL_A,   ARRAY_COUNT(CTRL_CHAL_A));
    path_build(&w->paths[PATH_CHAL_B_L],   CTRL_CHAL_B,   ARRAY_COUNT(CTRL_CHAL_B));
    path_build(&w->paths[PATH_CHAL_C_L],   CTRL_CHAL_C,   ARRAY_COUNT(CTRL_CHAL_C));
    path_build(&w->paths[PATH_CHAL_D_L],   CTRL_CHAL_D,   ARRAY_COUNT(CTRL_CHAL_D));
    path_build(&w->paths[PATH_CHAL_E_L],   CTRL_CHAL_E,   ARRAY_COUNT(CTRL_CHAL_E));
    path_build(&w->paths[PATH_CHAL_F_L],   CTRL_CHAL_F,   ARRAY_COUNT(CTRL_CHAL_F));
    path_mirror(&w->paths[PATH_TOP_DIVE_R], &w->paths[PATH_TOP_DIVE_L]);
    path_mirror(&w->paths[PATH_SWEEP_R],    &w->paths[PATH_SWEEP_L]);
    path_mirror(&w->paths[PATH_RETURN_R],   &w->paths[PATH_RETURN_L]);
    path_mirror(&w->paths[PATH_CORNER_R],   &w->paths[PATH_CORNER_L]);
    path_mirror(&w->paths[PATH_CHAL_A_R],   &w->paths[PATH_CHAL_A_L]);
    path_mirror(&w->paths[PATH_CHAL_B_R],   &w->paths[PATH_CHAL_B_L]);
    path_mirror(&w->paths[PATH_CHAL_C_R],   &w->paths[PATH_CHAL_C_L]);
    path_mirror(&w->paths[PATH_CHAL_D_R],   &w->paths[PATH_CHAL_D_L]);
    path_mirror(&w->paths[PATH_CHAL_E_R],   &w->paths[PATH_CHAL_E_L]);
    path_mirror(&w->paths[PATH_CHAL_F_R],   &w->paths[PATH_CHAL_F_L]);

    w->show_paths      = false;
    w->attacks_enabled = true;
    w->attacks_paused  = false;
    w->captive_holder  = -1;

    /* Seeded once, here rather than in wave_restart, so successive stages get
       different attack patterns while a whole run stays reproducible. */
    w->rng = 0x5A17C0DEu + wave_seed_base * 0x9E3779B9u;

    wave_restart(w, 1, 0);
}

/* Once per stage rather than per tick, so a stage's character is fixed when it
   starts and cannot drift under it. */
static void set_difficulty(Wave *w, int stage)
{
    if (stage < 1) stage = 1;
    w->stage = stage;

    float t = (float)(stage - 1) / (float)RAMP_STAGES;
    if (t > 1.0f) t = 1.0f;

    w->attack_interval = ATTACK_INTERVAL +
        (int)((ATTACK_INTERVAL_END - ATTACK_INTERVAL) * t);
    w->dive_speed      = DIVE_SPEED_1 + (DIVE_SPEED_END - DIVE_SPEED_1) * t;
    w->diver_cap       = DIVER_CAP_1 +
        (int)((DIVER_CAP_END - DIVER_CAP_1) * t + 0.5f);
    w->fire_chance_in  = FIRE_CHANCE_IN +
        (int)((FIRE_CHANCE_IN_END - FIRE_CHANCE_IN) * t);
    w->burst_len       = (int)(BURST_LEN_END * t + 0.5f);
    w->entry_fire      = (stage >= 2) ? w->fire_chance_in * ENTRY_FIRE_RATIO : 0;

    /* Attack curves are unlocked rather than ramped. Stage one flies the one
       shape the game has always flown, so a first wave is still the thing a
       player learns the game on; the steeper and less readable ones arrive
       once there is something to be surprised by. */
    w->dive_shapes = 1;
    if (stage >= 2) w->dive_shapes = 2;
    if (stage >= 4) w->dive_shapes = 3;
    if (stage >= 8) w->dive_shapes = DIVE_SHAPES;
    w->sway_period     = SWAY_PERIOD_1 + (SWAY_PERIOD_END - SWAY_PERIOD_1) * t;
}

/* A boss and its escorts share one path and count once - the unit the cap is
   written in, since three groups is three attacks to dodge either way. */
static int dive_groups(const Wave *w)
{
    int n = 0;
    for (int i = 0; i < MAX_DIVERS; ++i) if (w->dive_refs[i] > 0) ++n;
    return n;
}

/* A slot with the block's sway applied; formation_slot_pos gives its home.
   Keeping the two apart is what stops sway disturbing an arriving enemy - the
   join curve is built in slot space and the offset added to its output. */
static Vec2 slot_pos(const Wave *w, int slot)
{
    Vec2 v = formation_slot_pos(slot);
    v.x += w->sway;
    v.y += w->lift;
    return v;
}

/* Everything the two kinds of stage share. */
static void wave_reset_common(Wave *w)
{
    w->tick = 0;
    w->next_attack = 0;
    w->dives_boss = w->dives_butterfly = w->dives_bee = 0;
    for (int i = 0; i < DIVE_SHAPES; ++i) w->dives_by_shape[i] = 0;
    w->captive_holder = -1;
    w->min_lane_gap = 1e9f;
    w->shot_max_deg = 0.0f;
    w->peak_divers  = 0;
    w->max_convoy   = 0;
    w->max_convoy_high = 0;
    w->shots_fired  = 0;
    w->shots_on_entry = 0;
    w->park_off_min =  1e9f;
    w->park_off_max = -1e9f;
    memset(w->convoy_run, 0, sizeof w->convoy_run);
    w->burst_left   = 0;
    w->last_side    = 0;
    w->sway         = 0.0f;
    w->lift         = 0.0f;
    w->entries_held = false;
    for (int i = 0; i < MAX_DIVERS; ++i) w->dive_refs[i] = 0;
    for (int i = 0; i < PATH_COUNT; ++i)  w->lane_free[i] = 0;
    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) w->shot[i].alive = false;
    /* From the stage rather than the generator, so a given stage always
       assembles the same way. */
    int set = w->entry_set;

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        const Flight *fl = &ENTRIES[set][i / FLIGHT_SIZE];
        int within = i % FLIGHT_SIZE;

        e->shape       = s_slots[i].shape;
        e->state       = ENEMY_WAITING;
        e->slot        = i;
        e->path        = (within & 1) ? fl->path_b : fl->path_a;
        e->launch_tick = fl->start_tick + within * fl->spacing;
        e->s           = 0.0f;
        e->speed       = ENTRY_SPEED;
        e->pos         = w->paths[e->path].pt[0];
        e->pos.x      += e->lane_dx;
        e->heading     = HEADING_S;
        e->join_t      = 0.0f;
        e->join_rate   = 0.0f;
        e->dive_path    = -1;
        e->dive_s       = 0.0f;
        e->dive_lead    = 0.0f;
        e->dive_lateral = 0.0f;
        e->dive_formup  = 1.0f;
        e->hits         = 0;
        e->beam_t       = 0;
        e->has_captive  = false;
        e->returning    = false;
    }
}

void wave_restart(Wave *w, int stage, int entry)
{
    w->challenge      = false;
    w->challenge_hits = 0;
    w->chal_peak_groups = 0;
    w->chal_peak_flyers = 0;
    w->chal_quiet       = 0;
    w->chal_quiet_run   = 0;
    w->chal_last_seen   = 0;
    w->entry_set      = ((entry % ENTRY_SETS) + ENTRY_SETS) % ENTRY_SETS;
    set_difficulty(w, stage);
    wave_reset_common(w);
}

/* A challenging round: which two passes the flyers fly, the rhythm they arrive
   on, and how fast. Naming only the left-hand lane of each pair is enough,
   since every right-hand id follows its left-hand one - see PathId.
   
   The rounds differ in shape rather than in difficulty. A bonus stage pays the
   same either way, so what varies is the pattern to be read: A and B are the
   original descent and climb, C crosses the screen sideways, and D corkscrews
   through the middle. Pairing them differently each time means no two rounds
   present the same problem even where they share a pass. */
typedef struct {
    PathId lane;
    float  dx;           /* moved sideways from where the shape was authored */
} ChalLane;

typedef struct {
    ChalLane lanes[4];   /* the four lanes, in the order the groups take them */
    int      pair_pause; /* empty screen between one pair of groups and the next */
    int      within_gap; /* ticks between flyers inside a group                  */
    float    speed;      /* multiple of the entry speed                          */
} ChallengeRound;

/* within_gap has a floor, and it is not a matter of taste: the gun fires once
   every FIRE_COOLDOWN ticks and the five flyers of a group arrive one behind
   the other, so a group spaced tighter than that arrives faster than it can be
   shot at. The shooting still works out over a whole pair - there is a long
   tail after the last one lands - but in the moment you are always behind,
   which is what made the fourth round feel impossible rather than hard. */
#define CHAL_MIN_GAP FIRE_COOLDOWN

/* The four lanes a round flies, in the order its groups take them. */
static const ChalLane *chal_lanes(int variant);

static const ChallengeRound CHAL_ROUNDS[SHP_BONUS_COUNT] = {
    /* The first stays as it was: mirrored sweeps across the whole screen, and
       beatable from the middle. It is the one that teaches what a bonus round
       is, and the introduction is not the place to hide the lesson. */
    { { { PATH_CHAL_A_L,   0.0f }, { PATH_CHAL_A_R,   0.0f },
        { PATH_CHAL_B_L,   0.0f }, { PATH_CHAL_B_R,   0.0f } }, 54, 13, 1.15f },

    /* Four columns, alternating shapes. */
    { { { PATH_CHAL_E_L, -66.0f }, { PATH_CHAL_F_L, -21.0f },
        { PATH_CHAL_E_L,  24.0f }, { PATH_CHAL_F_L,  67.0f } }, 48, 13, 1.25f },

    /* Two columns close together on the left and two spread right, so the
       halves of the round want different footwork. */
    { { { PATH_CHAL_F_L, -70.0f }, { PATH_CHAL_F_L, -34.0f },
        { PATH_CHAL_E_L,  26.0f }, { PATH_CHAL_E_L,  74.0f } }, 60, 15, 1.05f },

    /* One sweep still crossing the screen, to keep a round that cannot be
       stood out entirely, with three columns around it. */
    { { { PATH_CHAL_E_L, -58.0f }, { PATH_CHAL_C_L,   0.0f },
        { PATH_CHAL_F_L,  32.0f }, { PATH_CHAL_E_L,  70.0f } }, 42, 14, 1.15f },
};

static const ChalLane *chal_lanes(int variant)
{
    int r = ((variant % SHP_BONUS_COUNT) + SHP_BONUS_COUNT) % SHP_BONUS_COUNT;
    return CHAL_ROUNDS[r].lanes;
}

/* How far either side of a column a shot still kills, and the band of the
   screen a shot can realistically reach something in. */
#define SHOT_REACH  10.0f
#define SHOOT_TOP   60.0f

/* The leftmost column a fighter can stand in. It is stopped by its own width,
   and a paired one is twice as wide - so the outer sixteen pixels are not
   somewhere the answer is allowed to be. Scanning from the screen edge instead
   had round 4 answering x=8, which no paired fighter can reach. */
#define REACH_EDGE(dual)  (CELL / 2.0f + ((dual) ? 8.0f : 0.0f))

static bool lane_reaches(const Wave *w, ChalLane lane, float x, bool dual)
{
    const Path *p = &w->paths[lane.lane];
    for (int i = 0; i < p->n; ++i) {
        if (p->pt[i].y < SHOOT_TOP || p->pt[i].y > (float)PLAYER_Y) continue;
        float d = fabsf(p->pt[i].x + lane.dx - x);
        if (d <= SHOT_REACH) return true;
        /* A paired fighter fires from two hulls 8px either side of centre. */
        if (dual && fabsf(d - 8.0f) <= SHOT_REACH) return true;
    }
    return false;
}

int wave_challenge_coverage(const Wave *w, int variant, float *best_x,
                            int *at_centre, bool dual)
{
    const ChalLane *lanes = chal_lanes(variant);

    int best = 0;
    float at = GAME_W * 0.5f;

    float edge = REACH_EDGE(dual);
    for (float x = edge; x <= GAME_W - edge; x += 2.0f) {
        int covered = 0;
        for (int g = 0; g < MAX_ENEMIES / CHAL_GROUP; ++g) {
            if (lane_reaches(w, lanes[g % 4], x, dual)) ++covered;
        }
        if (covered > best) { best = covered; at = x; }
        if (at_centre && fabsf(x - GAME_W * 0.5f) < 1.0f) *at_centre = covered;
    }

    if (best_x) *best_x = at;
    return best;
}

void wave_restart_challenge(Wave *w, int stage, int variant)
{
    set_difficulty(w, stage);
    wave_reset_common(w);
    w->challenge      = true;
    w->challenge_hits = 0;
    w->entry_fire     = 0;   /* a bonus round is a shooting gallery, not a fight */

    if (variant < 0) variant = 0;
    int r = variant % SHP_BONUS_COUNT;
    w->chal_variant = r;
    const ChallengeRound *round = &CHAL_ROUNDS[r];

    ShapeId shape = (ShapeId)(SHP_BONUS_FIRST + r);

    /* Later rounds fly faster on top of whatever pace the pattern itself asks
       for. Without this, cycling back to the first pattern at stage 19 would
       be a step backwards - the round table sets a round's character, and the
       stage sets how hard that character is to deal with. */
    float t = (float)(stage - 1) / (float)RAMP_STAGES;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float speed = ENTRY_SPEED * round->speed * CHAL_SPEED_TRIM
                * (1.0f + CHAL_SPEED_RAMP * t);

    const ChalLane *lanes = chal_lanes(variant);

    /* Eight groups of five, flown as four pairs: a lane and its mirror in the
       air together, and nothing else until both have left the screen.
     *
     * The arcade never put more than two groups up at once and left a clear
     * gap between pairs, and this is why. A bonus round pays for catching the
     * whole pattern, so it has to be a pattern you can see the whole of - with
     * groups launched on a fixed cadence, three or four were in flight at any
     * moment and there was no reading it, only shooting at the nearest thing.
     *
     * A pair is done when the slower of its two lanes has carried its last
     * flyer off the end, so the wait is derived rather than guessed - the lanes
     * differ in length and the speed ramps with the stage, and a fixed number
     * would go wrong at both ends of that. */
    int start[MAX_ENEMIES / CHAL_GROUP], at = 0;

    for (int g = 0; g < (int)ARRAY_COUNT(start); g += 2) {
        float longest = 0.0f;
        for (int k = 0; k < 2 && g + k < (int)ARRAY_COUNT(start); ++k) {
            float len = w->paths[lanes[(g + k) % 4].lane].length;
            if (len > longest) longest = len;
        }
        start[g] = at;
        if (g + 1 < (int)ARRAY_COUNT(start)) start[g + 1] = at;

        at += (int)(longest / speed)
            + (CHAL_GROUP - 1) * round->within_gap
            + round->pair_pause;
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        int group  = i / CHAL_GROUP;
        int within = i % CHAL_GROUP;

        e->shape       = shape;
        e->state       = ENEMY_WAITING;
        e->path        = lanes[group % 4].lane;
        e->lane_dx     = lanes[group % 4].dx;
        e->launch_tick = start[group] + within * round->within_gap;
        e->speed       = speed;
        e->pos         = w->paths[e->path].pt[0];
        e->heading     = HEADING_S;
    }
}

bool wave_is_challenge(const Wave *w)
{
    return w->challenge;
}

int wave_challenge_hits(const Wave *w)
{
    return w->challenge_hits;
}

/* ---------------------------------------------------------------- update */

/* Sets up the curve that carries an enemy off the end of its shared entry path
   and into its own slot.

   The two tangents are what make this read as flight rather than a slide. The
   departure tangent is the path's own exit direction, so the curve continues
   the motion instead of kinking away from it. The arrival tangent points due
   north, which does double duty: the enemy comes to rest already facing the
   way it will sit in formation, and because the heading is read off the curve,
   it turns gradually across the whole approach rather than pivoting on the
   spot. Both tangents are scaled by the gap being crossed - Hermite tangents
   are magnitudes as well as directions, and short hops need shallow curves. */
/* The curve from wherever an enemy is, flying whatever way it is facing, into
   its formation slot. Split out from begin_join below because two quite
   different things need it: an enemy reaching the end of an entry path, and a
   Boss Galaga that has just shut its tractor beam and wants to go home. */
static void begin_join_at(Enemy *e, Vec2 end, float exit)
{
    float rad    = exit * (float)M_PI / 180.0f;
    Vec2 target  = formation_slot_pos(e->slot);

    float dx   = target.x - end.x;
    float dy   = target.y - end.y;
    float gap  = sqrtf(dx * dx + dy * dy);
    float pull = gap * 1.15f;

    e->join_p0 = end;
    e->join_t0.x =  sinf(rad) * pull;   /* the path's exit direction */
    e->join_t0.y = -cosf(rad) * pull;
    e->join_p1 = target;
    e->join_t1.x = 0.0f;                /* arrive heading due north */
    e->join_t1.y = -pull;

    /* Time the approach by its actual length so every enemy crosses at the
       same speed no matter how far its slot is, entering at flight speed and
       decelerating to a stop. The factor of two pairs with the ease-out above,
       whose starting slope is 2. */
    float len = hermite_length(e->join_p0, e->join_t0, e->join_p1, e->join_t1);
    float ticks = (2.0f * len) / e->speed;
    if (ticks < 1.0f) ticks = 1.0f;

    e->join_rate = 1.0f / ticks;
    e->join_t    = 0.0f;
    e->state     = ENEMY_TO_SLOT;
}

/* Smoothstep, for easing an escort off its slot and on to station. */
static float ease(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* Puts one enemy on to a dive path at a fixed station within its group. The
   whole group advances the same distance each tick and samples the path at its
   own offset, so the shape they make is carried round the turns with them
   rather than being a fixed screen-space offset that stops making sense the
   moment the path bends.

   An escort does not start on station - it starts wherever it was parked - so
   it eases across during the first stretch of the dive, which reads as the trio
   forming up as it drops out of the formation. */
static void join_dive(Wave *w, Enemy *e, int path_idx, float lead, float lateral)
{
    e->dive_path    = path_idx;
    e->dive_s       = 0.0f;
    e->dive_lead    = lead;
    e->dive_lateral = lateral;
    e->dive_from    = e->pos;
    e->dive_formup  = (lead == 0.0f && lateral == 0.0f) ? 1.0f : 0.0f;
    e->state        = ENEMY_DIVING;
    ++w->dive_refs[path_idx];

    if (is_boss(e->shape))              ++w->dives_boss;
    else if (e->shape == SHP_BUTTERFLY) ++w->dives_butterfly;
    else                                ++w->dives_bee;
}

static bool is_boss(ShapeId s)
{
    return s == SHP_BOSS;
}

/* Sends the next attack, preferring a parked boss - a boss dive drags two
   butterflies with it, which is the formation's signature attack. */
enum { TYPE_BOSS, TYPE_BUTTERFLY, TYPE_BEE, TYPE_COUNT };

static int type_of(ShapeId s)
{
    if (is_boss(s))          return TYPE_BOSS;
    if (s == SHP_BUTTERFLY)  return TYPE_BUTTERFLY;
    return TYPE_BEE;
}

static void launch_attack(Wave *w, float player_x)
{
    int ready[TYPE_COUNT][MAX_ENEMIES];
    int n[TYPE_COUNT] = { 0, 0, 0 };

    /* Inside a burst, only the far half of the formation is in the running.
       A dive breaks towards its nearer edge, so two leaders on the same side
       fly near-identical curves; alternating makes them diverge by
       construction.

       The filter has to come before the type is chosen. Picking the type first
       and then looking for a member on the far side means a burst landing on
       the bosses - four of them, often all one side - finds nobody and falls
       back to the near half, which is exactly the case that convoys.

       Straight about what this is worth: the burst gap alone already clears
       the convoying line, and without this the worst pair over stages 2 to 24
       flies together for 16 ticks rather than 9. So it is not load-bearing. It
       is kept because halving the residue is cheap, and because a burst that
       alternates sides fans out across the screen instead of arriving from one
       half of the formation - which is worth having on its own. */
    int prefer = (w->burst_left > 0) ? -w->last_side : 0;

    for (int pass = 0; pass < 2; ++pass) {
        n[0] = n[1] = n[2] = 0;
        for (int i = 0; i < MAX_ENEMIES; ++i) {
            const Enemy *e = &w->enemies[i];
            if (e->state != ENEMY_FORMED) continue;
            if (prefer != 0 &&
                ((e->pos.x < GAME_W * 0.5f) ? -1 : 1) != prefer) continue;
            int t = type_of(e->shape);
            ready[t][n[t]++] = i;
        }
        if (n[0] + n[1] + n[2] > 0) break;
        prefer = 0;   /* nothing parked over there; take whatever is up */
    }
    if (n[0] + n[1] + n[2] == 0) return;

    /* Share of attacks each type leads. Choosing a type first, then a member,
       is what keeps the bottom of the formation involved - picking uniformly
       from every parked enemy means any one of the twenty bees rarely gets a
       turn. As weighted, a boss attacks about twice as often as a bee. */
    static const int WEIGHT[TYPE_COUNT] = { 20, 30, 50 };

    int pool = 0;
    for (int t = 0; t < TYPE_COUNT; ++t) if (n[t] > 0) pool += WEIGHT[t];

    int roll = rng_below(w, pool);
    int type = -1;
    for (int t = 0; t < TYPE_COUNT; ++t) {
        if (n[t] == 0) continue;
        if (roll < WEIGHT[t]) { type = t; break; }
        roll -= WEIGHT[t];
    }
    if (type < 0) {   /* only reachable through rounding; take whatever is up */
        for (int t = 0; t < TYPE_COUNT; ++t) if (n[t] > 0) { type = t; break; }
    }

    Enemy *leader = &w->enemies[ready[type][rng_below(w, n[type])]];

    /* A boss with no captive already in hand sometimes goes for the fighter
       instead of shooting at it. Kept to a minority of boss attacks: the beam
       ties the boss up for three seconds and is the most dangerous thing on
       the board, so it wants to be an event rather than the routine. */
    if (type == TYPE_BOSS && w->captive_holder < 0 && rng_below(w, 3) == 0) {
        leader->beam_from = leader->pos;
        leader->beam_pos.x = player_x;
        leader->beam_pos.y = (float)BEAM_HOVER_Y;
        /* Keep the cone on screen even when the fighter is against an edge. */
        if (leader->beam_pos.x < BEAM_BOT_HW)          leader->beam_pos.x = BEAM_BOT_HW;
        if (leader->beam_pos.x > GAME_W - BEAM_BOT_HW) leader->beam_pos.x = GAME_W - BEAM_BOT_HW;
        leader->beam_t = 0;
        leader->state  = ENEMY_BEAMING;
        audio_play(SFX_BEAM);
        ++w->dives_boss;
        return;
    }

    /* Dropped rather than queued: holding it would fire a burst the moment a
       group landed, which is a spike where the ramp wants a rate. */
    if (dive_groups(w) >= w->diver_cap) return;

    int p = dive_path_alloc(w);
    if (p < 0) return;   /* pool full; the next attack will get a slot */

    /* Break towards the nearer edge, so a dive opens out across the screen
       rather than immediately crossing the whole formation. */
    int side = (leader->pos.x < GAME_W * 0.5f) ? -1 : 1;
    w->last_side = side;
    int shape = rng_below(w, w->dive_shapes);
    w->path_shape[p] = shape;
    build_dive_path(&w->dive_paths[p], leader->pos, side, player_x, shape,
                    aim_pick(w));
    if (shape >= 0 && shape < DIVE_SHAPES) ++w->dives_by_shape[shape];

    /* One swoop per group, not per enemy: a trio leaves on the same tick, so
       three copies would just be one sound at three times the volume. The
       overlap worth having comes from separate attacks a burst apart. */
    audio_play(SFX_DIVE);

    join_dive(w, leader, p, 0.0f, 0.0f);
    if (type != TYPE_BOSS) return;

    /* The two parked butterflies nearest the boss's column, so the trio that
       dives together also looked like one standing still. They take station
       ahead and to either side - the boss is the trailing apex. */
    float bx = leader->pos.x;
    for (int slot = 0; slot < 2; ++slot) {
        int   best = -1;
        float best_d = 1e9f;
        for (int i = 0; i < MAX_ENEMIES; ++i) {
            Enemy *e = &w->enemies[i];
            if (e->state != ENEMY_FORMED) continue;
            if (e->shape != SHP_BUTTERFLY) continue;
            float d = fabsf(e->pos.x - bx);
            if (d < best_d) { best_d = d; best = i; }
        }
        if (best < 0) break;
        join_dive(w, &w->enemies[best], p,
                  ESCORT_LEAD, slot == 0 ? -ESCORT_SPREAD : ESCORT_SPREAD);
    }
}

static void track_lane_gap(Wave *w);
static void track_convoys(Wave *w);
static bool on_camera(Vec2 p);

/* What a bonus round actually put on screen, as opposed to what its schedule
   intended. Two groups at a time with a clear gap between pairs is the rule the
   arcade kept to, and the only way to know it holds across four patterns and a
   speed that ramps is to count. */
static void track_challenge(Wave *w)
{
    int seen[MAX_ENEMIES / CHAL_GROUP] = { 0 };
    int flyers = 0, groups = 0, waiting = 0;

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (e->state == ENEMY_WAITING) { ++waiting; continue; }
        if (e->state != ENEMY_ENTERING) continue;
        if (!on_camera(e->pos)) continue;
        ++flyers;
        if (!seen[i / CHAL_GROUP]++) ++groups;
    }

    if (groups > w->chal_peak_groups) w->chal_peak_groups = groups;
    if (flyers > w->chal_peak_flyers) w->chal_peak_flyers = flyers;

    /* Between pairs only: not before the first has flown, and not after the
       last, when an empty screen is the round being over rather than a gap. */
    if (flyers > 0) {
        w->chal_quiet_run = 0;
        w->chal_last_seen = w->tick;
    } else if (w->chal_last_seen > 0 && waiting > 0) {
        if (++w->chal_quiet_run > w->chal_quiet) w->chal_quiet = w->chal_quiet_run;
    }
}

/* The arcade pays more for a boss killed with its escort intact, and a group
   shares one dive path, so counting them is asking who else is on it. */
static int escorts_on(const Wave *w, int path_idx)
{
    if (path_idx < 0) return 0;
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (e->state == ENEMY_DIVING && e->dive_path == path_idx
            && e->dive_lead != 0.0f) ++n;
    }
    return n;
}

bool wave_hit(Wave *w, int index, int *score, int *popup)
{
    *score = 0;
    *popup = 0;
    if (index < 0 || index >= MAX_ENEMIES) return false;

    Enemy *e = &w->enemies[index];
    if (e->state == ENEMY_DEAD || e->state == ENEMY_WAITING) return false;

    /* A Boss Galaga shrugs off the first shot. Nothing swaps here: `hits` is
       what the draw picks its palette from, so damage is a recolour rather
       than a second set of artwork. */
    if (is_boss(e->shape) && e->hits == 0) {
        e->hits = 1;
        return false;
    }

    /* A bonus round pays a flat rate and keeps count; there are no ranks and
       nothing is in formation, so the usual table does not apply. */
    if (w->challenge) {
        ++w->challenge_hits;
        *score = CHALLENGE_HIT_SCORE;
        e->state = ENEMY_DEAD;
        return true;
    }

    bool diving = (e->state == ENEMY_DIVING);

    if (is_boss(e->shape)) {
        if (!diving) {
            *score = 150;
        } else {
            /* The escort still flying with it is what the bonus is paid for. */
            switch (escorts_on(w, e->dive_path)) {
            case 0:  *score =  400; break;
            case 1:  *score =  800; break;
            default: *score = 1600; break;
            }
        }
        *popup = *score;   /* boss kills are the ones the arcade puts on screen */
    } else if (e->shape == SHP_BUTTERFLY) {
        *score = diving ? 160 : 80;
    } else {
        *score = diving ? 100 : 50;
    }

    if (e->dive_path >= 0) {
        dive_path_release(w, e->dive_path);
        e->dive_path = -1;
    }

    /* Shooting the captor gives the fighter back; the game notices by watching
       the holder across the call. */
    if (e->has_captive) {
        e->has_captive = false;
        w->captive_holder = -1;
    }

    e->state = ENEMY_DEAD;
    return true;
}

/* Sends a missile from a diver towards where the fighter flies. */
static void enemy_fire(Wave *w, const Enemy *e, float player_x)
{
    float dy = (float)PLAYER_Y - e->pos.y;
    if (dy < FIRE_MIN_HEIGHT) return;   /* too low down to take a fair shot */

    float dx = player_x - e->pos.x;

    /* Hold the shot inside the cone. Clamping the sideways reach against the
       drop rather than clamping an angle keeps it to one comparison and no
       trigonometry. */
    float reach = dy * FIRE_MAX_SLOPE;
    if (dx >  reach) dx =  reach;
    if (dx < -reach) dx = -reach;

    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) {
        EnemyShot *s = &w->shot[i];
        if (s->alive) continue;

        s->alive = true;
        s->pos   = e->pos;
        s->vel.x = dx / len * ENEMY_SHOT_SPEED;
        s->vel.y = dy / len * ENEMY_SHOT_SPEED;

        float deg = (float)(atan2(fabs((double)dx), (double)dy) * 180.0 / M_PI);
        if (deg > w->shot_max_deg) w->shot_max_deg = deg;

        /* After the aim rules, not before: they turn plenty of would-be shots
           away, and a sound with no missile has nothing to explain it. */
        ++w->shots_fired;
        audio_play(SFX_ENEMY_FIRE);
        return;
    }
}

/* Queues an enemy for the return lane on its own side, whether its dive ran
   out or the whole wave was recalled. Queued rather than joined immediately:
   escorts share a station and finish on the same tick, so all three entered the
   lane on top of each other. A departure slot spaces out any such coincidence,
   and the waiting happens off-screen. */
/* An enemy that has run out of entry path joins from where the path ended,
   still flying the way the path was going. */
static void begin_join(Enemy *e, const Path *p)
{
    begin_join_at(e, path_point(p, p->length), path_heading(p, p->length));
}

static void send_home(Wave *w, Enemy *e)
{
    if (e->dive_path >= 0) {
        dive_path_release(w, e->dive_path);
        e->dive_path = -1;
    }

    Vec2 slot = formation_slot_pos(e->slot);
    int  lane = (slot.x < GAME_W * 0.5f) ? PATH_RETURN_L : PATH_RETURN_R;

    int depart = w->tick;
    if (w->lane_free[lane] > depart) depart = w->lane_free[lane];
    w->lane_free[lane] = depart + RETURN_SPACING;

    e->path        = lane;
    e->launch_tick = depart;
    e->speed       = ENTRY_SPEED;
    e->state       = ENEMY_WAITING;
    e->returning   = true;
}

void wave_recall(Wave *w)
{
    wave_recall_except(w, -1);
}

void wave_recall_except(Wave *w, int keep)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (i == keep) continue;
        Enemy *e = &w->enemies[i];
        if (e->state == ENEMY_DIVING) send_home(w, e);

        /* A boss mid-capture shuts up shop too, but keeps anything it has
           already taken - the captive belongs to it now. It climbs back to its
           slot from where it hangs rather than going round by the return lane,
           for the same reason a beam that ends normally does: it never left the
           top half of the screen, and sending it round makes it disappear and
           come back in the corner. */
        if (e->state == ENEMY_BEAMING) {
            if (e->dive_path >= 0) {
                dive_path_release(w, e->dive_path);
                e->dive_path = -1;
            }
            e->speed = ENTRY_SPEED;
            begin_join_at(e, e->pos, e->heading);
        }
    }
    wave_clear_shots(w);
}

void wave_pause_attacks(Wave *w, bool paused)
{
    w->attacks_paused = paused;
}

bool wave_area_clear(const Wave *w, Vec2 at, float radius)
{
    float r2 = radius * radius;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (e->state == ENEMY_DEAD || e->state == ENEMY_WAITING) continue;
        float dx = e->pos.x - at.x, dy = e->pos.y - at.y;
        if (dx * dx + dy * dy <= r2) return false;
    }
    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) {
        const EnemyShot *s = &w->shot[i];
        if (!s->alive) continue;
        float dx = s->pos.x - at.x, dy = s->pos.y - at.y;
        if (dx * dx + dy * dy <= r2) return false;
    }
    return true;
}

void wave_update(Wave *w, float player_x)
{
    ++w->tick;

    /* The parked block drifts from side to side. One offset moves the whole
       formation, so the shape of it never distorts - the arcade sways the
       block as a unit and the columns stay columns. Starting from a sine at
       tick 0 means the offset is zero while the wave is still flying in, so
       the entry paths still end exactly on their slots. */
    w->sway = SWAY_AMP * sinf((float)w->tick *
                              (2.0f * (float)M_PI) / w->sway_period);

    /* Attacks only start once the wave is up, the way a stage does. The check
       has to latch: the instant the first enemy leaves its slot the formation
       is no longer complete, so testing it every tick would fire one attack
       and then never another. A non-zero next_attack is that latch. */
    if (w->attacks_enabled && !w->attacks_paused && !w->challenge) {
        if (w->next_attack == 0 && wave_all_formed(w)) {
            w->next_attack = w->tick + w->attack_interval;
        }
        if (w->next_attack != 0 && w->tick >= w->next_attack) {
            launch_attack(w, player_x);

            /* Either we are partway through a burst, in which case the next
               one follows close behind, or the burst is done and the wave goes
               quiet for a full interval. */
            if (w->burst_left > 0) {
                --w->burst_left;
                w->next_attack = w->tick + BURST_GAP;
            } else {
                w->burst_left  = w->burst_len;
                w->next_attack = w->tick + w->attack_interval;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        const Path *p = &w->paths[e->path];

        switch (e->state) {
        case ENEMY_WAITING:
            /* A held schedule waits with the enemy rather than running on
               without it. The wave's tick keeps advancing while a board is
               being packed away and brought back - two hundred and fifty of
               them across a handover - and a launch time left behind by that
               is a launch time already past, so every enemy still waiting went
               out on the single tick the hold lifted, stacked on top of one
               another. Carrying the times along keeps the gaps between them. */
            /* A board packing itself away still lets its own divers home.
                They are formation members caught out of place, and leaving
                them parked off-screen means they turn up unannounced when the
                board comes back - which is exactly what they did. */
            if (w->entries_held && !e->returning) { ++e->launch_tick; break; }

            if (w->tick >= e->launch_tick) {
                e->state   = ENEMY_ENTERING;
                e->s       = 0.0f;
                e->pos     = path_point(p, 0.0f);
                e->pos.x  += e->lane_dx;   /* same shift the rest of the lane gets */
                e->heading = path_heading(p, 0.0f);
            }
            break;

        case ENEMY_ENTERING:
            e->s += e->speed;
            if (e->s >= p->length) {
                /* In a bonus round the path runs off the edge of the screen
                   and that is the end of it - nothing forms up, and anything
                   not shot on the way through simply escapes. */
                if (w->challenge) e->state = ENEMY_DEAD;
                else              begin_join(e, p);
            } else {
                e->pos     = path_point(p, e->s);
                e->pos.x  += e->lane_dx;
                e->heading = path_heading(p, e->s);

                /* From the second stage on the wave shoots on its way in, as
                   the arcade's does. The same aim rules apply as to a diver,
                   so an entry shot is no less dodgeable than any other. */
                if (w->entry_fire && rng_below(w, w->entry_fire) == 0) {
                    int before = w->shots_fired;
                    enemy_fire(w, e, player_x);
                    if (w->shots_fired > before) ++w->shots_on_entry;
                }
            }
            break;

        case ENEMY_TO_SLOT: {
            e->join_t += e->join_rate;
            if (e->join_t > 1.0f) e->join_t = 1.0f;

            /* Ease out only - entered at flight speed, at rest at the slot.
               u'(0) = 2, hence the rate set against twice the length. */
            float u = e->join_t * (2.0f - e->join_t);

            e->pos = hermite_point(e->join_p0, e->join_t0,
                                   e->join_p1, e->join_t1, u);

            /* Heading from the curve's own tangent, so it points where it is
               going. At u = 1 that is join_t1, due north: it arrives already
               sitting the way it will park. */
            Vec2 tan = hermite_tangent(e->join_p0, e->join_t0,
                                       e->join_p1, e->join_t1, u);
            e->heading = heading_from_vec(tan.x, tan.y);

            e->pos.x += w->sway;
            e->pos.y += w->lift;

            if (e->join_t >= 1.0f) {
                e->state     = ENEMY_FORMED;
                e->pos       = slot_pos(w, e->slot);
                e->heading   = HEADING_N;
                e->returning = false;
            }
            break;
        }

        case ENEMY_FORMED:
            e->pos = slot_pos(w, e->slot);
            break;

        case ENEMY_DEAD:
            break;

        case ENEMY_BEAMING: {
            ++e->beam_t;

            if (e->beam_t <= BEAM_DESCEND) {
                /* Slide down out of formation, easing in and out so it settles
                   rather than stopping dead. */
                float t = (float)e->beam_t / (float)BEAM_DESCEND;
                float k = t * t * (3.0f - 2.0f * t);
                e->pos.x = e->beam_from.x + (e->beam_pos.x - e->beam_from.x) * k;
                e->pos.y = e->beam_from.y + (e->beam_pos.y - e->beam_from.y) * k;
                e->heading = HEADING_S;
            } else {
                e->pos = e->beam_pos;
                e->heading = HEADING_S;
            }

            /* Beam shut: climb back to the slot from where it is hovering,
               rather than going round by the return lane the way a diver does.
               A diver ends its run off the bottom of the screen and genuinely
               has to come back in over the top; a boss that has been hanging
               below the formation the whole time has not gone anywhere, and
               sending it round made it vanish and reappear in the corner -
               with the captured fighter along with it.

               It leaves on the heading it is already flying, which is south.
               The join curve therefore swings it down and around before it
               climbs, so it turns rather than flipping about-face on one
               frame. */
            if (e->beam_t >= BEAM_TOTAL) {
                if (e->dive_path >= 0) {
                    dive_path_release(w, e->dive_path);
                    e->dive_path = -1;
                }
                e->speed = ENTRY_SPEED;
                begin_join_at(e, e->pos, e->heading);
            }
            break;
        }

        case ENEMY_DIVING: {
            const Path *dp = &w->dive_paths[e->dive_path];
            e->dive_s += w->dive_speed;

            /* Every member of a group advances the same distance; where each
               one actually sits is that distance plus its own station. */
            float path_s = e->dive_s + e->dive_lead;
            if (path_s >= dp->length) {
                /* Off the bottom. Hand the path back and come round again from
                   the top: a return is just another entry, so it re-uses the
                   entering state and, after that, the same join home. */
                send_home(w, e);
            } else {
                Vec2  here = path_point(dp, path_s);
                float h    = path_heading(dp, path_s);

                /* Offset sideways along the path's normal, so the triangle
                   banks with the flight instead of staying axis-aligned. */
                if (e->dive_lateral != 0.0f) {
                    float rad = h * (float)M_PI / 180.0f;
                    here.x += cosf(rad) * e->dive_lateral;
                    here.y += sinf(rad) * e->dive_lateral;
                }

                if (e->dive_formup < 1.0f) {
                    e->dive_formup += 1.0f / 26.0f;
                    float k = ease(e->dive_formup);
                    here.x = e->dive_from.x + (here.x - e->dive_from.x) * k;
                    here.y = e->dive_from.y + (here.y - e->dive_from.y) * k;
                }

                e->pos     = here;
                e->heading = h;

                if (rng_below(w, w->fire_chance_in) == 0) enemy_fire(w, e, player_x);
            }
            break;
        }
        }
    }

    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) {
        EnemyShot *s = &w->shot[i];
        if (!s->alive) continue;
        s->pos.x += s->vel.x;
        s->pos.y += s->vel.y;
        if (s->pos.y < -CELL || s->pos.y > GAME_H + CELL ||
            s->pos.x < -CELL || s->pos.x > GAME_W + CELL) {
            s->alive = false;
        }
    }

    track_lane_gap(w);
    track_convoys(w);
    if (w->challenge) track_challenge(w);

    if (wave_track && w->challenge) {
        for (int i = 0; i < MAX_ENEMIES; ++i) {
            const Enemy *e = &w->enemies[i];
            if (e->state != ENEMY_ENTERING) continue;
            printf("F %d %d %d %.2f %.2f\n", w->tick, i, i / CHAL_GROUP,
                   e->pos.x, e->pos.y);
        }
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (e->state != ENEMY_FORMED) continue;
        float off = e->pos.x - formation_slot_pos(e->slot).x;
        if (off < w->park_off_min) w->park_off_min = off;
        if (off > w->park_off_max) w->park_off_max = off;
    }

    int groups = dive_groups(w);
    if (groups > w->peak_divers) w->peak_divers = groups;
}

void wave_print_stats(const Wave *w)
{
    printf("stage %d: attack every %d ticks, dive speed %.1f, cap %d group(s), "
           "fire 1 in %d, sway %.0fpx over %.0f ticks\n",
           w->stage, w->attack_interval, w->dive_speed, w->diver_cap,
           w->fire_chance_in, SWAY_AMP, w->sway_period);
    printf("peak dive groups in the air at once: %d (cap %d)\n",
           w->peak_divers, w->diver_cap);
    static const char *SHAPE[4] = { "peel", "loop", "cross", "plunge" };
    printf("longest two dive groups flew together: %d ticks (%d = convoying)\n",
           w->max_convoy, CONVOY_TICKS);
    printf("  of which above the approach, where there is no excuse: %d ticks\n",
           w->max_convoy_high);
    if (w->max_convoy > 0) {
        printf("  worst was %s and %s at y %.0f (the fighter flies at %d)\n",
               SHAPE[w->convoy_shape[0] & 3], SHAPE[w->convoy_shape[1] & 3],
               w->convoy_y, PLAYER_Y);
    }
    printf("parked enemies sat between %+.1f and %+.1f px from their slots\n",
           w->park_off_min, w->park_off_max);
    printf("closest two enemies sharing a path: %.1f px\n", w->min_lane_gap);
    {
        int   ws = -1;
        float sideways = wave_dive_sideways(w, &ws);
        printf("fastest an attack crosses the fighter's row sideways: %.1f px/tick "
               "(%s), against the fighter's %.1f\n",
               sideways, wave_dive_name(ws), (double)PLAYER_SPEED);
    }
    {
        int   worst = -1;
        float low   = 0.0f;
        float clear = wave_entry_clearance(w, &worst, &low);
        printf("entry paths clear the fighter's row by %.0f px, worst is %s "
               "(under %.0f is an unavoidable hit)\n",
               clear, wave_path_name(worst), (double)ENEMY_SHIP_RADIUS);
        printf("  and while off the side of the screen they get no lower than "
               "y=%.0f, %.0f px above the row\n", low, (double)PLAYER_Y - low);
    }
    if (w->challenge) {
        printf("challenging stage - no entry set, and nothing shoots\n");
        printf("  most on screen at once: %d groups, %d flyers (2 groups is the rule)\n",
               w->chal_peak_groups, w->chal_peak_flyers);
        printf("  longest empty screen between pairs: %d ticks (%.2fs)\n",
               w->chal_quiet, w->chal_quiet / 60.0f);
        {
            int gap = CHAL_ROUNDS[w->chal_variant].within_gap;
            printf("  flyers arrive every %d ticks, the gun cycles every %d%s\n",
                   gap, FIRE_COOLDOWN,
                   gap < CHAL_MIN_GAP ? "  <-- arriving faster than it can fire"
                                      : "");
        }
        {
            int   mid  = 0;
            float at   = 0.0f;
            int   best = wave_challenge_coverage(w, w->chal_variant, &at, &mid, true);
            printf("  a paired fighter that never moves reaches %d of 8 groups "
                   "from the middle, %d at best (x=%.0f)\n", mid, best, at);

            /* A perfect pays 10,000, so it has to stay possible. Two lanes
               fly at once and there is no time to cross the screen between
               them, so each pair needs one column that reaches both - the
               round asks you to know four spots, not to be in two places. */
            {
                const ChalLane *pl = chal_lanes(w->chal_variant);
                for (int pair = 0; pair < 2; ++pair) {
                    float lo = -1.0f, hi = -1.0f;
                    float edge = REACH_EDGE(true);
                    for (float x = edge; x <= GAME_W - edge; x += 2.0f) {
                        if (lane_reaches(w, pl[pair * 2], x, true) &&
                            lane_reaches(w, pl[pair * 2 + 1], x, true)) {
                            if (lo < 0.0f) lo = x;
                            hi = x;
                        }
                    }
                    if (lo >= 0.0f) {
                        printf("    pair %d: stand between x=%.0f and %.0f "
                               "to reach both lanes\n", pair, lo, hi);
                    } else {
                        printf("    pair %d: NO column reaches both - "
                               "a perfect is impossible\n", pair);
                    }
                }
            }

            const ChalLane *ll = chal_lanes(w->chal_variant);
            for (int i = 0; i < 4; ++i) {
                const Path *pp = &w->paths[ll[i].lane];
                float lo = 1e9f, hi = -1e9f;
                for (int k = 0; k < pp->n; ++k) {
                    if (pp->pt[k].y < SHOOT_TOP || pp->pt[k].y > (float)PLAYER_Y)
                        continue;
                    if (pp->pt[k].x < lo) lo = pp->pt[k].x;
                    if (pp->pt[k].x > hi) hi = pp->pt[k].x;
                }
                /* And how hard it turns. A flyer that doubles back reads as
                   stopping dead, which is the fault the dive curves had twice
                   and which reading control points did not catch either time. */
                float worst = 0.0f;
                for (int k = 2; k < pp->n; ++k) {
                    float ax = pp->pt[k - 1].x - pp->pt[k - 2].x;
                    float ay = pp->pt[k - 1].y - pp->pt[k - 2].y;
                    float bx = pp->pt[k].x - pp->pt[k - 1].x;
                    float by = pp->pt[k].y - pp->pt[k - 1].y;
                    float la = sqrtf(ax * ax + ay * ay);
                    float lb = sqrtf(bx * bx + by * by);
                    if (la < 1e-4f || lb < 1e-4f) continue;
                    float c = (ax * bx + ay * by) / (la * lb);
                    if (c < -1.0f) c = -1.0f;
                    if (c >  1.0f) c =  1.0f;
                    float deg = acosf(c) * 180.0f / (float)M_PI;
                    if (deg > worst) worst = deg;
                }
                printf("    lane %d sweeps x %.0f..%.0f (%.0f wide), "
                       "sharpest turn %.0f deg\n",
                       i, lo + ll[i].dx, hi + ll[i].dx, hi - lo, worst);
            }
        }
    } else {
        printf("entry set: %d of %d\n", w->entry_set, ENTRY_SETS);
    }

    /* Printed before the early return below: a run short enough to measure an
       entry has no dives in it, and the entry numbers are the point of it. */
    printf("missiles fired: %d total, %d of them on the way in\n",
           w->shots_fired, w->shots_on_entry);
    printf("steepest missile fired: %.1f deg off straight down (90 = undodgeable)\n",
           w->shot_max_deg);
    {
        /* Worst case: fired from the minimum height, straight down. */
        float ticks = FIRE_MIN_HEIGHT / ENEMY_SHOT_SPEED;
        printf("  the shortest warning a missile can give is %.0f ticks "
               "(%.2fs), in which the fighter can move %.0f px\n",
               ticks, ticks / 60.0f, ticks * PLAYER_SPEED);
    }

    int total = w->dives_boss + w->dives_butterfly + w->dives_bee;
    if (total == 0) { printf("no dives yet\n"); return; }
    printf("dives after %d ticks: %d total\n", w->tick, total);
    printf("  boss       %4d  (%.1f%%)  over  4 slots\n",
           w->dives_boss, 100.0 * w->dives_boss / total);
    printf("  butterfly  %4d  (%.1f%%)  over 16 slots\n",
           w->dives_butterfly, 100.0 * w->dives_butterfly / total);
    printf("  bee        %4d  (%.1f%%)  over 20 slots\n",
           w->dives_bee, 100.0 * w->dives_bee / total);
    printf("attack curves in play: %d of %d\n", w->dive_shapes, DIVE_SHAPES);
    for (int i = 0; i < DIVE_SHAPES; ++i) {
        printf("  %-8s %4d  (%.1f%%)\n", wave_dive_name(i), w->dives_by_shape[i],
               100.0 * w->dives_by_shape[i] / total);
    }
}

int wave_divers(const Wave *w)
{
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (w->enemies[i].state == ENEMY_DIVING) ++n;
    }
    return n;
}

/* Closest approach between two enemies sharing a path this tick. Only enemies
   on the same path are compared: a diver cutting through the parked formation
   overlaps others all the time and that is expected, whereas two enemies on one
   lane converging is the bug this guards against. */
static void track_lane_gap(Wave *w)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *a = &w->enemies[i];
        if (a->state != ENEMY_ENTERING) continue;
        for (int j = i + 1; j < MAX_ENEMIES; ++j) {
            const Enemy *b = &w->enemies[j];
            if (b->state != ENEMY_ENTERING || b->path != a->path) continue;
            float dx = a->pos.x - b->pos.x, dy = a->pos.y - b->pos.y;
            float d  = sqrtf(dx * dx + dy * dy);
            if (d < w->min_lane_gap) w->min_lane_gap = d;
        }
    }
}

/* True while an enemy is somewhere the player can actually see it. */
static bool on_camera(Vec2 p)
{
    return p.y > -CELL && p.y < (float)GAME_H && p.x > -CELL && p.x < GAME_W + CELL;
}

/* How long each pair of dive groups has been close. Escorts are meant to fly
   close to their boss, so only different dive paths are compared, and only
   while both are on screen - this measures whether two attacks read as one
   smear, and sprites overlapping below the bottom edge read as nothing. */
static void track_convoys(Wave *w)
{
    float best[MAX_DIVERS][MAX_DIVERS];
    float at_y[MAX_DIVERS][MAX_DIVERS];
    int   high[MAX_DIVERS][MAX_DIVERS];
    for (int a = 0; a < MAX_DIVERS; ++a) {
        for (int b = 0; b < MAX_DIVERS; ++b) {
            best[a][b] = 1e9f;
            at_y[a][b] = 0.0f;
            high[a][b] = 0;
        }
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *ea = &w->enemies[i];
        if (ea->state != ENEMY_DIVING || ea->dive_path < 0) continue;
        if (!on_camera(ea->pos)) continue;
        for (int j = i + 1; j < MAX_ENEMIES; ++j) {
            const Enemy *eb = &w->enemies[j];
            if (eb->state != ENEMY_DIVING || eb->dive_path < 0) continue;
            if (eb->dive_path == ea->dive_path) continue;
            if (!on_camera(eb->pos)) continue;

            int a = ea->dive_path, b = eb->dive_path;
            if (a > b) { int t = a; a = b; b = t; }

            float dx = ea->pos.x - eb->pos.x, dy = ea->pos.y - eb->pos.y;
            float d  = sqrtf(dx * dx + dy * dy);
            if (d < best[a][b]) {
                best[a][b] = d;
                at_y[a][b] = (ea->pos.y + eb->pos.y) * 0.5f;
                /* Two attacks converging on the approach is not the same fault
                   as two attacks flying the length of the screen as one. The
                   approach is a corridor by construction now - every curve
                   lines up on the fighter's column and comes down it at a
                   bounded angle, which is the whole fairness rule - so some
                   crowding down there is the price of that rule and not
                   something to tune away. Above it there is no such excuse, and
                   that is the number to hold. */
                high[a][b] = (ea->pos.y < AIM_Y_MIN && eb->pos.y < AIM_Y_MIN);
            }
        }
    }

    for (int a = 0; a < MAX_DIVERS; ++a) {
        for (int b = a + 1; b < MAX_DIVERS; ++b) {
            if (best[a][b] < CONVOY_DIST) {
                if (high[a][b] && w->convoy_run[a][b] + 1 > w->max_convoy_high) {
                    w->max_convoy_high = w->convoy_run[a][b] + 1;
                }
                if (++w->convoy_run[a][b] > w->max_convoy) {
                    w->max_convoy      = w->convoy_run[a][b];
                    w->convoy_y        = at_y[a][b];
                    w->convoy_shape[0] = w->path_shape[a];
                    w->convoy_shape[1] = w->path_shape[b];
                }
            } else {
                w->convoy_run[a][b] = 0;
            }
        }
    }
}

/* How fast a board leaves or arrives. Brisk enough not to be a wait, slow
   enough to read as a formation moving rather than a cut. */
#define LIFT_SPEED 3.2f

bool wave_lift(Wave *w, float target)
{
    float d = target - w->lift;
    if (d > -LIFT_SPEED && d < LIFT_SPEED) {
        w->lift = target;
        return true;
    }
    w->lift += (d > 0.0f) ? LIFT_SPEED : -LIFT_SPEED;
    return false;
}

void wave_hold_entries(Wave *w, bool hold)
{
    w->entries_held = hold;
}

void wave_print_unsettled(const Wave *w)
{
    static const char *NAME[] = {
        "waiting", "entering", "to-slot", "formed", "diving", "beaming", "dead"
    };
    int n[7] = { 0 };
    for (int i = 0; i < MAX_ENEMIES; ++i) n[w->enemies[i].state]++;
    printf("  still moving:");
    for (int i = 0; i < 7; ++i) {
        if (i == ENEMY_WAITING || i == ENEMY_FORMED || i == ENEMY_DEAD) continue;
        if (n[i]) printf(" %d %s", n[i], NAME[i]);
    }
    printf(" (entries %s)\n", w->entries_held ? "held" : "free");
}

bool wave_settled(const Wave *w)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];

        /* Anything on its way home counts as unsettled even while queued, or
           a board looks quiet with divers still parked off the top. */
        if (e->returning && e->state != ENEMY_DEAD) return false;

        switch (e->state) {
        case ENEMY_ENTERING:
        case ENEMY_TO_SLOT:
        case ENEMY_DIVING:
        case ENEMY_BEAMING:
            return false;
        default:
            break;
        }
    }
    return true;
}

const char *wave_path_name(int path)
{
    static const char *NAME[PATH_COUNT] = {
        "top dive L", "top dive R", "sweep L", "sweep R",
        "return L",   "return R",   "corner L", "corner R",
        "chal A L",   "chal A R",   "chal B L", "chal B R",
        "chal C L",   "chal C R",   "chal D L", "chal D R",
    };
    return (path >= 0 && path < PATH_COUNT) ? NAME[path] : "?";
}

float wave_entry_clearance(const Wave *w, int *worst_path, float *offscreen_low)
{
    float worst = 1e9f;
    float low   = -1e9f;
    int   which = -1;

    /* The entry paths only. A bonus round's flyers cannot touch the fighter,
       and the dive paths are meant to reach it. */
    for (int p = PATH_TOP_DIVE_L; p <= PATH_CORNER_R; ++p) {
        const Path *path = &w->paths[p];
        for (int i = 0; i < path->n; ++i) {
            Vec2 q = path->pt[i];

            float d = fabsf(q.y - (float)PLAYER_Y);
            if (d < worst) { worst = d; which = p; }

            /* And how far down anything gets while still off the side: an
               enemy that flies in along the fighter's row cannot be dodged,
               because the first you see of it is the collision. */
            if ((q.x < 0.0f || q.x > (float)GAME_W) && q.y > low) low = q.y;
        }
    }

    if (worst_path)    *worst_path    = which;
    if (offscreen_low) *offscreen_low = low;
    return worst;
}

void wave_rearm_attacks(Wave *w)
{
    w->next_attack = 0;
}

bool wave_all_formed(const Wave *w)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        EnemyState st = w->enemies[i].state;
        if (st != ENEMY_FORMED && st != ENEMY_DEAD) return false;
    }
    return true;
}

void wave_clear_shots(Wave *w)
{
    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) w->shot[i].alive = false;
}

bool wave_cleared(const Wave *w)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (w->enemies[i].state != ENEMY_DEAD) return false;
    }
    return true;
}

/* ------------------------------------------------------------------ draw */

static void draw_path(Gfx *g, const Path *p, SDL_Color c)
{
    SDL_SetRenderDrawColor(g->renderer, c.r, c.g, c.b, c.a);
    SDL_Point pts[PATH_MAX_SAMPLES];
    int n = 0;
    for (int i = 0; i < p->n; ++i) {
        pts[n].x = (int)p->pt[i].x;
        pts[n].y = (int)p->pt[i].y;
        ++n;
    }
    if (n > 1) SDL_RenderDrawLines(g->renderer, pts, n);
}

void wave_draw(Gfx *g, const Wave *w)
{
    if (w->show_paths) {
        static const SDL_Color trace[PATH_COUNT] = {
            {  60, 120,  60, 255 }, {  60, 120,  60, 255 },
            {  40,  80, 140, 255 }, {  40,  80, 140, 255 },
            { 100,  80, 130, 255 }, { 100,  80, 130, 255 },
        };
        for (int i = 0; i < PATH_COUNT; ++i) draw_path(g, &w->paths[i], trace[i]);

        static const SDL_Color dive_trace = { 150, 50, 50, 255 };
        for (int i = 0; i < MAX_DIVERS; ++i) {
            if (w->dive_refs[i] > 0) draw_path(g, &w->dive_paths[i], dive_trace);
        }
    }

    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) {
        const EnemyShot *s = &w->shot[i];
        if (!s->alive) continue;
        /* The missile points along its velocity - no direction rose to pick a
           frame from any more, the shape just turns. */
        shape_draw(g, SHP_ENEMY_SHOT, s->pos,
                   heading_from_vec(s->vel.x, s->vel.y), 1.0f);
    }

    /* Beams first, so the boss and anything caught in one draw over them. */
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (e->state != ENEMY_BEAMING) continue;

        float open = beam_open(e);
        if (open <= 0.01f) continue;

        /* Bands sliding down the cone. The movement is the whole effect: a
           static gradient reads as a shape, a moving one reads as a beam. */
        for (int b = 0; b < BEAM_BANDS; ++b) {
            float phase = (float)((w->tick * 2 + b * 9) % (BEAM_BANDS * 9))
                        / (float)(BEAM_BANDS * 9);
            float t0 = phase;
            float t1 = phase + 0.055f;
            if (t1 > 1.0f) continue;

            float d0 = t0 * BEAM_LEN * open;
            float d1 = t1 * BEAM_LEN * open;
            float w0 = beam_half_width(d0, open);
            float w1 = beam_half_width(d1, open);

            Vec2 quad[4] = {
                { e->pos.x - w0, e->pos.y + d0 },
                { e->pos.x + w0, e->pos.y + d0 },
                { e->pos.x + w1, e->pos.y + d1 },
                { e->pos.x - w1, e->pos.y + d1 },
            };
            SDL_Color c = (b & 1) ? (SDL_Color){ 120, 230, 255, 0 }
                                  : (SDL_Color){  60, 120, 255, 0 };
            c.a = (Uint8)(210.0f * open * (1.0f - t0 * 0.55f));
            shape_draw_poly(g, quad, 4, c);
        }
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (e->state == ENEMY_WAITING || e->state == ENEMY_DEAD) continue;

        /* A whisker along each heading. The flyers are near enough symmetric
           that backwards looks like forwards without it. */
        if (w->show_paths) {
            float rad = e->heading * (float)M_PI / 180.0f;
            SDL_SetRenderDrawColor(g->renderer, 255, 255, 0, 255);
            SDL_RenderDrawLine(g->renderer,
                               (int)e->pos.x, (int)e->pos.y,
                               (int)(e->pos.x + sinf(rad) * 11.0f),
                               (int)(e->pos.y - cosf(rad) * 11.0f));
        }

        /* A damaged boss is the same shape in a different palette. */
        const ShapePalette *pal =
            (is_boss(e->shape) && e->hits > 0) ? &SHAPE_PAL_BOSS_HIT : NULL;

        float heading = e->heading;
        float scale   = 1.0f;

        if (e->state == ENEMY_FORMED) {
            /* Parked enemies sit upright and breathe. The arcade alternated
               two poses; a slow pulse reads the same with one shape. */
            heading = HEADING_N;
            scale   = 1.0f + 0.07f * sinf((float)(w->tick + i * 7) * 0.09f);
        }

        shape_draw_pal(g, e->shape, e->pos, heading, scale, pal, 1.0f);

        /* The taken fighter rides with its captor, in enemy colours and turned
           to face the way they do - it belongs to them now. */
        if (e->has_captive) {
            Vec2 o   = captive_offset(e);
            Vec2 cap = { e->pos.x + o.x, e->pos.y + o.y };
            shape_draw_pal(g, SHP_FIGHTER, cap, e->heading + 180.0f, 1.0f,
                           &SHAPE_PAL_FIGHTER_CAPTURED, 1.0f);
        }
    }
}

const char *wave_dive_name(int shape)
{
    static const char *NAME[DIVE_SHAPES] = { "peel", "loop", "cross", "plunge" };
    return (shape >= 0 && shape < DIVE_SHAPES) ? NAME[shape] : "?";
}

float wave_dive_sideways(const Wave *w, int *worst_shape)
{
    /* The corners of the problem rather than every case: top and bottom rows,
       both edges and the middle, the fighter at either wall and centre, both
       break directions. A curve that behaves at all of those behaves. */
    static const float FROM_Y[]   = { (float)FORM_Y,
                                      (float)(FORM_Y + (FORM_ROWS - 1) * FORM_PITCH) };
    static const float FROM_X[]   = { (float)FORM_X, GAME_W * 0.5f,
                                      (float)(GAME_W - FORM_X) };
    static const float PLAYER_X[] = { 16.0f, GAME_W * 0.5f, (float)(GAME_W - 16) };
    static const float AIM_OFF[]  = { -(float)AIM_SPREAD, 0.0f, (float)AIM_SPREAD };

    float worst = 0.0f;
    int   which = -1;

    (void)w;
    for (int shape = 0; shape < DIVE_SHAPES; ++shape) {
    for (int a = 0; a < ARRAY_COUNT(FROM_Y); ++a) {
    for (int b = 0; b < ARRAY_COUNT(FROM_X); ++b) {
    for (int c = 0; c < ARRAY_COUNT(PLAYER_X); ++c) {
    for (int d = 0; d < ARRAY_COUNT(AIM_OFF); ++d) {
    for (int e = 0; e < 2; ++e) {
    for (int side = -1; side <= 1; side += 2) {
        static Path p;
        /* The full tilt at both ends of the altitude range: the tilt is the
           worst case for the bound, the altitudes for where it lands. */
        Aim  aim  = { AIM_OFF[d], 1.0f,
                      AIM_Y_MIN + (float)((e % 2) * AIM_Y_VARY) };
        Vec2 from = { FROM_X[b], FROM_Y[a] };
        build_dive_path(&p, from, side, PLAYER_X[c], shape, aim);

        for (int i = 1; i < p.n; ++i) {
            if (fabsf(p.pt[i].y - (float)PLAYER_Y) > FIGHTER_BAND) continue;

            /* Evenly spaced by arc length, so the sideways share of a step is
               the sideways share of the speed. At the hardest dive speed. */
            float dx   = p.pt[i].x - p.pt[i - 1].x;
            float dy   = p.pt[i].y - p.pt[i - 1].y;
            float step = sqrtf(dx * dx + dy * dy);
            if (step < 1e-6f) continue;

            float sideways = fabsf(dx) / step * DIVE_SPEED_END;
            if (sideways > worst) { worst = sideways; which = shape; }
        }
    }}}}}}}

    if (worst_shape) *worst_shape = which;
    return worst;
}

void wave_dump_dives(void)
{
    /* Every knob that changes the shape, so the picture shows the family
       rather than one member. Reading control points has twice failed to catch
       a curve that doubled back; plotting the polyline caught both. */
    static const Vec2  FROM[]  = { { 56.0f, 44.0f }, { 104.0f, 76.0f },
                                   { 168.0f, 108.0f } };
    static const float AIM_Y[] = { AIM_Y_MIN, AIM_Y_MIN + AIM_Y_VARY };
    float player_x = 112.0f;

    for (int shape = 0; shape < DIVE_SHAPES; ++shape) {
        for (int f = 0; f < ARRAY_COUNT(FROM); ++f) {
            for (int side = -1; side <= 1; side += 2) {
                for (int a = 0; a < ARRAY_COUNT(AIM_Y); ++a) {
                    static Path p;
                    Aim aim = { 0.0f, 1.0f, AIM_Y[a] };
                    build_dive_path(&p, FROM[f], side, player_x, shape, aim);
                    printf("SHAPE %d %s from %.0f,%.0f side %d aimy %.0f n=%d len=%.1f\n",
                           shape, wave_dive_name(shape), FROM[f].x, FROM[f].y,
                           side, AIM_Y[a], p.n, p.length);
                    for (int i = 0; i < p.n; ++i)
                        printf("PT %.2f %.2f\n", p.pt[i].x, p.pt[i].y);
                }
            }
        }
    }
}
