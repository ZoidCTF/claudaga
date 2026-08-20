#include "formation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ slots */

/* The wave as it ends up: four Boss Galaga across the top, two ranks of eight
   butterflies, then two ranks of ten bees. Slots are numbered in this order,
   and the flights below simply take them eight at a time - which lands the
   bosses and the first butterflies in the opening flight, as on the real
   board. */
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

/* Control points are in screen pixels and the curve passes through every one,
   so these read as the shape they draw. Both start well outside the screen so
   an enemy is already up to speed by the time it becomes visible. */

/* Straight down the middle from above, then a full loop low on the screen -
   the signature Galaga move. Both paths stop below the formation rather than
   running up into it: the shared flight ends where the enemies would have to
   diverge, and the curve into each individual slot takes over from there.
   Approaching every slot from beneath is also what lets that curve arrive
   pointing north without hooking back on itself. The loop is deliberately
   round and tight rather than a lazy oval; that is what reads as a loop at
   this size. */
static const Vec2 CTRL_TOP_DIVE[] = {
    { 100, -28 }, { 100,  56 }, { 100, 132 }, { 100, 190 },
    {  88, 226 }, {  60, 240 }, {  32, 226 }, {  22, 196 },
    {  34, 168 }, {  62, 158 }, {  94, 150 },
};

/* Up from below the bottom-left corner, climbing the left edge, into a loop
   across the top, ending back below the formation like the dive does.
   Deliberately shaped to occupy a different band of the screen than the dive,
   so two flights in the air at once stay legible. */
static const Vec2 CTRL_SWEEP[] = {
    { -24, 312 }, {  10, 272 }, {  36, 226 }, {  50, 178 },
    {  54, 132 }, {  70,  96 }, { 102,  78 }, { 134,  90 },
    { 144, 120 }, { 122, 142 }, {  96, 134 },
};

/* The way home after a dive. A diver leaves the bottom of the screen, so it
   comes back in over the top, drops down the outside of the formation where it
   cannot cut through the enemies still parked there, and turns back up beneath
   it. Ending below the formation heading north is the whole point: it leaves
   the enemy in exactly the state an arriving enemy is in, so the same join
   curve finishes the trip. */
static const Vec2 CTRL_RETURN[] = {
    {  16, -24 }, {  18,  40 }, {  22, 104 }, {  36, 148 },
    {  68, 166 }, {  96, 156 },
};

/* Challenging-stage passes. Unlike every other path here these begin and end
   off-screen: nothing forms up in a bonus round, so a flyer that survives its
   run simply leaves. A big sweeping S down the screen, and a climb through a
   loop and out of the top. */
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

static const Flight FLIGHTS[FLIGHT_COUNT] = {
    { PATH_TOP_DIVE_L, PATH_TOP_DIVE_R,   0, 11 },
    { PATH_SWEEP_L,    PATH_SWEEP_R,    130, 11 },
    { PATH_TOP_DIVE_R, PATH_TOP_DIVE_L, 260, 11 },
    { PATH_SWEEP_R,    PATH_SWEEP_L,    390, 11 },
    { PATH_SWEEP_L,    PATH_SWEEP_R,    520, 11 },
};

#define ENTRY_SPEED  2.6f    /* pixels per tick along an entry or return path */
#define DIVE_SPEED   3.1f    /* attacks come in noticeably faster */

/* Ticks between attacks once the wave is up. */
#define ATTACK_INTERVAL 105

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

   Aim is deliberately not a straight line to the fighter. The fighter is
   pinned to one row and can only move along it, so a missile arriving flat
   along that row cannot be dodged at all - and an enemy that has swooped down
   level with it was firing almost exactly that. Two rules keep a shot fair:
   the enemy has to be some way above the fighter to take one, and the aim is
   clamped into a cone about straight down, so there is always enough downward
   travel to step aside from. */
#define ENEMY_SHOT_SPEED  2.3f
#define FIRE_CHANCE_IN    150     /* per diving enemy, per tick */
#define FIRE_MIN_HEIGHT   44.0f   /* must be this far above the fighter's row */
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

/* How far down the screen the boss hangs while the beam is open. The pair of
   numbers has to be chosen together: hover plus length must actually reach
   PLAYER_Y or the beam cannot catch anything, which is exactly what the first
   attempt got wrong - it stopped eighty pixels short of the fighter's row and
   no capture was possible at all. */
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

Vec2 wave_captive_pos(const Wave *w)
{
    Vec2 v = { GAME_W * 0.5f, FORM_Y };
    int h = w->captive_holder;
    if (h < 0 || h >= MAX_ENEMIES) return v;
    v = w->enemies[h].pos;
    v.y += 15.0f;    /* it rides underneath */
    return v;
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

/* An attack, built where the enemy is rather than from a fixed template: it
   peels out of the slot, curls over, and sweeps down across the player before
   leaving the bottom of the screen. `side` decides which way it breaks, and
   aiming the lower half at the player's column is what stops a wave of dives
   from tracing the same line every time. */
static void build_dive_path(Path *out, Vec2 from, int side, float player_x)
{
    float s = (float)side;
    Vec2 c[7];
    c[0] = from;
    c[1].x = from.x + s * 13.0f;  c[1].y = from.y -  9.0f;   /* rise out    */
    c[2].x = from.x + s * 35.0f;  c[2].y = from.y + 15.0f;   /* curl over   */
    c[3].x = from.x + s * 30.0f;  c[3].y = from.y + 62.0f;   /* fall away   */
    c[4].x = (from.x + player_x) * 0.5f - s * 22.0f;
    c[4].y = from.y + 122.0f;                                /* swing in    */
    c[5].x = player_x + s * 26.0f; c[5].y = 246.0f;          /* past the    */
    c[6].x = player_x - s * 10.0f; c[6].y = 324.0f;          /* player, out */
    path_build(out, c, 7);
}

static int dive_path_alloc(Wave *w)
{
    for (int i = 0; i < MAX_DIVERS; ++i) {
        if (w->dive_refs[i] == 0) return i;
    }
    return -1;
}

/* Called by each enemy as it finishes; the path goes back to the pool when the
   last of the group is off it. */
static void dive_path_release(Wave *w, int i)
{
    if (i >= 0 && i < MAX_DIVERS && w->dive_refs[i] > 0) --w->dive_refs[i];
}

/* ----------------------------------------------------------------- setup */

void wave_init(Wave *w)
{
    build_slots();

    path_build(&w->paths[PATH_TOP_DIVE_L], CTRL_TOP_DIVE, ARRAY_COUNT(CTRL_TOP_DIVE));
    path_build(&w->paths[PATH_SWEEP_L],    CTRL_SWEEP,    ARRAY_COUNT(CTRL_SWEEP));
    path_build(&w->paths[PATH_RETURN_L],   CTRL_RETURN,   ARRAY_COUNT(CTRL_RETURN));
    path_build(&w->paths[PATH_CHAL_A_L],   CTRL_CHAL_A,   ARRAY_COUNT(CTRL_CHAL_A));
    path_build(&w->paths[PATH_CHAL_B_L],   CTRL_CHAL_B,   ARRAY_COUNT(CTRL_CHAL_B));
    path_mirror(&w->paths[PATH_TOP_DIVE_R], &w->paths[PATH_TOP_DIVE_L]);
    path_mirror(&w->paths[PATH_SWEEP_R],    &w->paths[PATH_SWEEP_L]);
    path_mirror(&w->paths[PATH_RETURN_R],   &w->paths[PATH_RETURN_L]);
    path_mirror(&w->paths[PATH_CHAL_A_R],   &w->paths[PATH_CHAL_A_L]);
    path_mirror(&w->paths[PATH_CHAL_B_R],   &w->paths[PATH_CHAL_B_L]);

    w->show_paths      = false;
    w->attacks_enabled = true;
    w->attacks_paused  = false;
    w->captive_holder  = -1;

    /* Seeded once, here rather than in wave_restart, so successive stages get
       different attack patterns while a whole run stays reproducible. */
    w->rng = 0x5A17C0DEu;

    wave_restart(w);
}

/* Everything the two kinds of stage share. */
static void wave_reset_common(Wave *w)
{
    w->tick = 0;
    w->next_attack = 0;
    w->dives_boss = w->dives_butterfly = w->dives_bee = 0;
    w->captive_holder = -1;
    w->min_lane_gap = 1e9f;
    w->shot_max_deg = 0.0f;
    for (int i = 0; i < MAX_DIVERS; ++i) w->dive_refs[i] = 0;
    for (int i = 0; i < PATH_COUNT; ++i)  w->lane_free[i] = 0;
    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) w->shot[i].alive = false;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        const Flight *fl = &FLIGHTS[i / FLIGHT_SIZE];
        int within = i % FLIGHT_SIZE;

        e->shape       = s_slots[i].shape;
        e->state       = ENEMY_WAITING;
        e->slot        = i;
        e->path        = (within & 1) ? fl->path_b : fl->path_a;
        e->launch_tick = fl->start_tick + within * fl->spacing;
        e->s           = 0.0f;
        e->speed       = ENTRY_SPEED;
        e->pos         = w->paths[e->path].pt[0];
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
    }
}

void wave_restart(Wave *w)
{
    w->challenge      = false;
    w->challenge_hits = 0;
    wave_reset_common(w);
}

void wave_restart_challenge(Wave *w, int variant)
{
    wave_reset_common(w);
    w->challenge      = true;
    w->challenge_hits = 0;

    ShapeId shape = (ShapeId)(SHP_BONUS_FIRST + (variant % SHP_BONUS_COUNT));

    /* Eight groups of five, alternating the two passes and the side they come
       in on, so the screen is crossed from several directions at once. */
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        int group  = i / 5;
        int within = i % 5;

        static const PathId LANES[4] = {
            PATH_CHAL_A_L, PATH_CHAL_A_R, PATH_CHAL_B_L, PATH_CHAL_B_R,
        };

        e->shape       = shape;
        e->state       = ENEMY_WAITING;
        e->path        = LANES[(group + variant) % 4];
        e->launch_tick = group * 74 + within * 13;
        e->speed       = ENTRY_SPEED * 1.15f;
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
static void begin_join(Enemy *e, const Path *p)
{
    Vec2 end     = path_point(p, p->length);
    float exit   = path_heading(p, p->length);
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

/* Sends the next attack. A boss is preferred when one is parked, because a
   boss dive drags two butterflies down with it and that is the formation's
   signature attack; otherwise anything still sitting there will do. */
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

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &w->enemies[i];
        if (e->state != ENEMY_FORMED) continue;
        int t = type_of(e->shape);
        ready[t][n[t]++] = i;
    }
    if (n[0] + n[1] + n[2] == 0) return;

    /* Share of attacks each type leads. Choosing a type first, then a member
       of it, is what keeps the bottom of the formation involved: picking
       uniformly from every parked enemy means any one of the twenty bees
       almost never gets a turn, and preferring a boss outright is worse still,
       because with four of them one is nearly always available. Weighted this
       way, and counting the two butterflies an escorted boss drags down with
       it, an individual boss ends up attacking about twice as often as an
       individual bee - a skew in the right direction rather than a rout. */
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
        ++w->dives_boss;
        return;
    }

    int p = dive_path_alloc(w);
    if (p < 0) return;   /* pool full; the next attack will get a slot */

    /* Break towards the nearer edge, so a dive opens out across the screen
       rather than immediately crossing the whole formation. */
    int side = (leader->pos.x < GAME_W * 0.5f) ? -1 : 1;
    build_dive_path(&w->dive_paths[p], leader->pos, side, player_x);

    join_dive(w, leader, p, 0.0f, 0.0f);
    if (type != TYPE_BOSS) return;

    /* Escorts: the two parked butterflies nearest the boss's column, so the
       trio that dives together also looked like a group standing still. They
       take station ahead of the boss and to either side - the boss flies at
       the trailing apex of the triangle, not at its point. */
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

/* How many of a boss's escorts are still flying its path. The arcade pays more
   for a boss killed with its escort intact, and because a group shares one dive
   path, counting them is just a matter of asking who else is on it. */
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

    /* Shooting the boss that took your fighter is what gives it back. The
       game notices by watching the holder across the call. */
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
        return;
    }
}

/* Takes an enemy off whatever it is doing and queues it for the return lane
   on its own side. Used both when a dive runs out at the bottom of the screen
   and when the whole wave is recalled.

   Queueing rather than joining the lane straight away matters: a boss and its
   escorts finish within a few ticks of each other - the escorts share a station
   and so finish on the very same tick - and starting them all at the head of
   the same path put them exactly on top of one another the whole way back. A
   departure slot spaces them out, and does it for any two enemies that happen
   to coincide. Waiting happens off-screen, where nothing is drawn. */
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
}

void wave_recall(Wave *w)
{
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        if (e->state == ENEMY_DIVING) send_home(w, e);

        /* A boss mid-capture shuts up shop and leaves too, but keeps anything
           it has already taken - the captive belongs to it now. */
        if (e->state == ENEMY_BEAMING) send_home(w, e);
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

    /* Attacks only start once the wave is up, the way a stage does. The check
       has to latch: the instant the first enemy leaves its slot the formation
       is no longer complete, so testing it every tick would fire one attack
       and then never another. A non-zero next_attack is that latch. */
    if (w->attacks_enabled && !w->attacks_paused && !w->challenge) {
        if (w->next_attack == 0 && wave_all_formed(w)) {
            w->next_attack = w->tick + ATTACK_INTERVAL;
        }
        if (w->next_attack != 0 && w->tick >= w->next_attack) {
            launch_attack(w, player_x);
            w->next_attack = w->tick + ATTACK_INTERVAL;
        }
    }

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *e = &w->enemies[i];
        const Path *p = &w->paths[e->path];

        switch (e->state) {
        case ENEMY_WAITING:
            if (w->tick >= e->launch_tick) {
                e->state   = ENEMY_ENTERING;
                e->s       = 0.0f;
                e->pos     = path_point(p, 0.0f);
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
                e->heading = path_heading(p, e->s);
            }
            break;

        case ENEMY_TO_SLOT: {
            e->join_t += e->join_rate;
            if (e->join_t > 1.0f) e->join_t = 1.0f;

            /* Ease out only: the curve is entered at flight speed and slows to
               rest at the slot. u'(0) = 2 and u'(1) = 0, which is why the rate
               below is set against twice the curve's length. */
            float u = e->join_t * (2.0f - e->join_t);

            e->pos = hermite_point(e->join_p0, e->join_t0,
                                   e->join_p1, e->join_t1, u);

            /* Heading comes from the curve's own tangent, so the enemy points
               where it is travelling for the whole approach instead of turning
               independently of it. At u = 1 the tangent is join_t1, which is
               due north - it arrives already sitting the way it will park. */
            Vec2 tan = hermite_tangent(e->join_p0, e->join_t0,
                                       e->join_p1, e->join_t1, u);
            e->heading = heading_from_vec(tan.x, tan.y);

            if (e->join_t >= 1.0f) {
                e->state   = ENEMY_FORMED;
                e->pos     = e->join_p1;
                e->heading = HEADING_N;
            }
            break;
        }

        case ENEMY_FORMED:
            e->pos = formation_slot_pos(e->slot);
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

            if (e->beam_t >= BEAM_TOTAL) send_home(w, e);
            break;
        }

        case ENEMY_DIVING: {
            const Path *dp = &w->dive_paths[e->dive_path];
            e->dive_s += DIVE_SPEED;

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

                if (rng_below(w, FIRE_CHANCE_IN) == 0) enemy_fire(w, e, player_x);
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
}

void wave_print_stats(const Wave *w)
{
    int total = w->dives_boss + w->dives_butterfly + w->dives_bee;
    if (total == 0) { printf("no dives yet\n"); return; }
    printf("dives after %d ticks: %d total\n", w->tick, total);
    printf("  boss       %4d  (%.1f%%)  over  4 slots\n",
           w->dives_boss, 100.0 * w->dives_boss / total);
    printf("  butterfly  %4d  (%.1f%%)  over 16 slots\n",
           w->dives_butterfly, 100.0 * w->dives_butterfly / total);
    printf("  bee        %4d  (%.1f%%)  over 20 slots\n",
           w->dives_bee, 100.0 * w->dives_bee / total);
    printf("closest two enemies sharing a path: %.1f px\n", w->min_lane_gap);
    printf("steepest missile fired: %.1f deg off straight down (90 = undodgeable)\n",
           w->shot_max_deg);
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

        /* A whisker along each enemy's heading. The flyers are near enough to
           symmetric that facing backwards looks much like facing forwards, so
           this is the quickest way to confirm they point where they fly. */
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
            /* Parked enemies sit upright and breathe. The sheet did this by
               alternating two drawn poses; with one shape that can turn and
               scale freely, a slow pulse reads as the same thing and needs no
               second drawing. */
            heading = HEADING_N;
            scale   = 1.0f + 0.07f * sinf((float)(w->tick + i * 7) * 0.09f);
        }

        shape_draw_pal(g, e->shape, e->pos, heading, scale, pal, 1.0f);

        /* The taken fighter rides under its captor, in enemy colours. */
        if (e->has_captive) {
            Vec2 cap = { e->pos.x, e->pos.y + 15.0f };
            shape_draw_pal(g, SHP_FIGHTER, cap, e->heading + 180.0f, 1.0f,
                           &SHAPE_PAL_FIGHTER_CAPTURED, 1.0f);
        }
    }
}
