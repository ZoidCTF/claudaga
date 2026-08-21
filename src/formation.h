#ifndef CLAUDAGA_FORMATION_H
#define CLAUDAGA_FORMATION_H

#include "gfx.h"
#include "shape.h"
#include "path.h"

/* The attack wave: how it assembles, and how it attacks.
 *
 * Forty enemies fill a 10x5 grid, but they do not start there. They arrive in
 * five flights of eight, each flight split into two streams that enter from
 * opposite sides and fly a scripted curve before peeling off into their slot.
 * Once the wave is assembled they begin diving at the player, and a dive ends
 * by re-entering from the top and rejoining formation - which is the same
 * problem as the original entry, so it reuses the same state and the same
 * join curve rather than inventing a second way home. */

#define FORM_COLS   10
#define FORM_ROWS   5
#define FORM_PITCH  16
#define MAX_ENEMIES 40   /* 4 bosses + 16 butterflies + 20 bees; see SLOT_RANKS */

/* Ten columns at a 16px pitch put 144px between the outer sprite centres. */
#define FORM_X ((GAME_W - (FORM_COLS - 1) * FORM_PITCH) / 2)
#define FORM_Y 44

/* How many enemies can be mid-dive at once. Each one needs a path of its own,
   built for where it happened to be sitting, so this bounds the pool below. */
#define MAX_DIVERS 6

typedef enum {
    ENEMY_WAITING,    /* off-screen, its flight has not launched yet */
    ENEMY_ENTERING,   /* flying an entry or return path */
    ENEMY_TO_SLOT,    /* peeling off the path into its formation slot */
    ENEMY_FORMED,     /* parked, flapping */
    ENEMY_DIVING,     /* attacking, on a path built when the dive began */
    ENEMY_BEAMING,    /* a boss hovering with its tractor beam open */
    ENEMY_DEAD        /* shot; the slot stays empty for the rest of the stage */
} EnemyState;

typedef struct {
    ShapeId    shape;
    EnemyState state;
    int        hits;         /* a Boss Galaga survives the first one */
    int        slot;
    int        path;         /* index into Wave.paths */
    int        launch_tick;
    float      s;            /* distance travelled along the path */
    float      speed;
    Vec2       pos;
    float      heading;

    /* The peel-off, as a Hermite curve from where the path ran out to the
       enemy's own slot. Storing the tangents lets the heading come from the
       curve itself, so the enemy always faces the way it is moving. */
    Vec2       join_p0, join_t0;
    Vec2       join_p1, join_t1;
    float      join_t;       /* 0..1 along the join */
    float      join_rate;    /* per-tick step, set from the curve's length */

    /* Attacking. A boss and its two escorts share one path and fly it at
       fixed stations, which is what holds the trio in a triangle through the
       turns: `dive_lead` is how far ahead of the leader this enemy flies and
       `dive_lateral` how far to the side, both zero for the leader itself. */
    int        dive_path;    /* slot in Wave.dive_paths, or -1 */
    float      dive_s;       /* the group's progress, same for all three */
    float      dive_lead;
    float      dive_lateral;
    float      dive_formup;  /* 0..1, easing an escort off its slot on to station */
    Vec2       dive_from;    /* where it was parked, for that ease-in */

    /* The capture routine: descend, open the beam, hold it, close, go home.
       One timer drives the lot; which phase it is in falls out of the value. */
    int        beam_t;
    Vec2       beam_from;    /* the slot it left    */
    Vec2       beam_pos;     /* where it hovers     */
    bool       has_captive;  /* carrying a taken fighter */

    /* On its way back from a dive rather than arriving for the first time. A
       board being packed away has to let these finish - they belong in the
       formation and were only out because they were attacking - while enemies
       that have never launched stay where they are. */
    bool       returning;
} Enemy;

/* Authored paths. The left-hand ones are written out; each right-hand one is
   its mirror, so a symmetric entry is only described once. */
/* A missile in flight. Its heading picks the sprite from the direction rose,
   so it is stored as a velocity rather than a speed and an angle. */
#define MAX_ENEMY_SHOTS 16

typedef struct {
    Vec2 pos, vel;
    bool alive;
} EnemyShot;

typedef enum {
    PATH_TOP_DIVE_L, PATH_TOP_DIVE_R,
    PATH_SWEEP_L,    PATH_SWEEP_R,
    PATH_RETURN_L,   PATH_RETURN_R,
    PATH_CORNER_L,   PATH_CORNER_R,   /* a diagonal from one top corner */
    /* Challenging-stage passes. Unlike the others these leave the screen
       rather than ending below the formation, and a bonus round picks two of
       the four. Every right-hand id follows its left-hand one, which is what
       lets a round name one lane and get the mirrored pair. */
    PATH_CHAL_A_L,   PATH_CHAL_A_R,
    PATH_CHAL_B_L,   PATH_CHAL_B_R,
    PATH_CHAL_C_L,   PATH_CHAL_C_R,
    PATH_CHAL_D_L,   PATH_CHAL_D_R,
    PATH_COUNT
} PathId;

typedef struct {
    Enemy enemies[MAX_ENEMIES];
    Path  paths[PATH_COUNT];

    /* Dive paths are built at runtime from wherever the leader was sitting, so
       they live in a pool that is handed out and returned. A path is shared by
       everyone flying it, so the pool is reference counted rather than a plain
       in-use flag - an escort finishes ahead of its boss and must not pull the
       path out from under it. */
    Path  dive_paths[MAX_DIVERS];
    int   dive_refs[MAX_DIVERS];
    int   path_shape[MAX_DIVERS];   /* which curve each pool slot is flying */

    /* Missiles the divers have fired. They outlive the enemy that fired them,
       so they belong to the wave rather than to an enemy. */
    EnemyShot shot[MAX_ENEMY_SHOTS];

    int   tick;
    int   next_attack;       /* tick the next attack is due */
    int   captive_holder;    /* enemy carrying a captured fighter, or -1 */

    /* Difficulty. Every one of these was a compile-time constant until stages
       started to differ from one another; they are now worked out once, from
       the stage number, when the wave resets. The constants they ramp between
       live in formation.c next to the rest of the tuning. */
    int   stage;
    int   attack_interval;   /* ticks between attacks          */
    float dive_speed;        /* pixels per tick down a dive    */
    int   diver_cap;         /* dive groups allowed at once    */
    int   fire_chance_in;    /* 1-in-N per diver per tick      */
    int   entry_fire;        /* 1-in-N per enemy flying in, 0 = never */
    int   entry_set;         /* which entry this wave flew in on      */
    int   dive_shapes;       /* how many attack curves are in play    */
    int   burst_len;         /* extra attacks tacked on to one */
    int   burst_left;        /* still owed on the current one  */
    int   last_side;         /* which edge the last dive broke towards */
    float sway_period;       /* ticks for one full sway cycle  */

    /* Where the parked block currently sits relative to its slots. The whole
       formation shares one offset, so this is a single number rather than
       something each enemy carries. */
    float sway;

    /* How far the whole parked block is displaced vertically from its slots.
       Zero is home; negative is off the top of the screen. This is how a board
       leaves and arrives between turns - the formation keeps its shape and
       simply translates, which is what "flies off in formation" means. */
    float lift;

    /* While held, nothing new launches on to an entry path. A board that is
       being packed away must stop sending more enemies out to fly, or the air
       never clears. Enemies already flying are unaffected; they finish. */
    bool  entries_held;

    /* A challenging stage: the wave flies through without ever forming up,
       never attacks and never fires, and anything not shot simply escapes. */
    bool  challenge;
    int   challenge_hits;

    /* The wave draws from its own generator rather than the global rand().
       Sharing one meant the attack mix depended on how many times the
       starfield had drawn that frame - so the distribution --stats measured in
       isolation was not the distribution the game actually played. */
    u32   rng;

    /* Earliest tick each path is free to launch another enemy on to. The
       return lanes need this: a boss and its escorts finish a dive at almost
       the same moment, and without a queue they would re-enter stacked on top
       of one another. */
    int   lane_free[PATH_COUNT];

    /* Tally of who has attacked, so the mix can be checked against intent
       rather than eyeballed off the screen. */
    int   dives_boss, dives_butterfly, dives_bee;

    /* And of which curve they flew. A shape that is in the table but never
       chosen is invisible from the outside - the entry sets had exactly that
       fault - so the count is kept rather than assumed. */
    int   dives_by_shape[4];

    /* Closest two enemies have come while flying the same path. Enemies
       finishing a dive together used to re-enter stacked, so this is the
       number that says whether the lane queue is doing its job. */
    float min_lane_gap;

    /* The longest two dive groups have spent flying on top of one another, in
       ticks. Bursts launch groups seconds apart from wherever they happen to be
       parked, so two leaders on the same side of the formation can end up on
       near-identical curves - which is the bug the formation already had once,
       arriving.

       Distance alone will not measure this. Two groups on genuinely different
       curves cross each other regularly, and a crossing puts them briefly at
       zero: the first attempt at this metric read 0.3px and meant nothing.
       What separates a crossing from a convoy is how long it lasts, so what is
       tracked is the run of consecutive ticks a pair of groups stays close. A
       crossing is two or three; anything travelling together is dozens. */
    /* How far a parked enemy has actually been seen sitting from its slot.
       The sway is a single offset applied in one place, so what is worth
       measuring is not the arithmetic but whether the block really moves: this
       is taken from the enemies themselves rather than from the offset. */
    float park_off_min, park_off_max;

    int   max_convoy;
    int   convoy_run[MAX_DIVERS][MAX_DIVERS];

    /* Where the worst one happened, and between which two curves. A number on
       its own says something is wrong; these say what to go and look at. */
    float convoy_y;
    int   convoy_shape[2];

    /* Most dive groups in the air at once. The difficulty ramp raises the
       ceiling on this, so it is the number that says the ramp is doing
       something rather than just being computed. */
    int   peak_divers;

    /* Missiles fired, and how many of them came from the wave on its way in
       rather than from an attack. Entry fire is easy to get wrong in the
       direction of far too much - forty enemies cross the screen during an
       entry - so it is a thing to count rather than eyeball. */
    int   shots_fired;
    int   shots_on_entry;

    /* Steepest missile fired, in degrees away from straight down. A shot that
       approaches 90 travels along the fighter's own row, which cannot be
       dodged, so this is the number that says the aim cone is holding. */
    float shot_max_deg;
    bool  show_paths;
    bool  attacks_enabled;   /* the A key; off means never attack        */
    bool  attacks_paused;    /* transient, while the player is not there */
} Wave;

void wave_init(Wave *w);       /* builds paths and slots; call once */

/* Sends everyone back off-screen to fly in again. `stage` is the stage number
   the wave is about to be, which is what sets the difficulty: the same forty
   enemies attack faster, more of them at once, and shoot more often as it
   climbs. */
/* `entry` selects which of the entry sets the wave flies in on. It is a count
   of ordinary stages rather than the stage number, and the caller works it out
   because the caller owns the schedule. That distinction is not fussiness: the
   challenging stages fall every fourth stage, which is exactly the number of
   entry sets, so indexing on the stage number aliased perfectly and one of the
   four never came up in a whole run. */
void wave_restart(Wave *w, int stage, int entry);

/* A challenging stage instead of an ordinary one. `variant` picks the round -
   which two passes the flyers fly, which flyer fields it, and the rhythm they
   arrive on. */
void wave_restart_challenge(Wave *w, int stage, int variant);

bool wave_is_challenge(const Wave *w);
int  wave_challenge_hits(const Wave *w);

/* What a bonus round pays: per flyer caught, and for catching all of them. */
#define CHALLENGE_HIT_SCORE 100
#define CHALLENGE_PERFECT   10000
void wave_update(Wave *w, float player_x);
void wave_draw(Gfx *g, const Wave *w);

bool wave_all_formed(const Wave *w);

/* Nothing is in the air: every enemy is parked, dead, or still waiting its
   turn to enter. Weaker than wave_all_formed on purpose - a board interrupted
   during its entry has enemies that have not launched yet, and waiting for
   those would wait forever. */
bool wave_settled(const Wave *w);

/* Re-arms the attack timer, so nothing dives until the formation is complete
   again. The latch that does this normally is only tripped once per wave; a
   board handed back mid-stage needs it tripped again. */
void wave_rearm_attacks(Wave *w);

/* Names what is still moving, for when a wait for quiet does not end. */
void wave_print_unsettled(const Wave *w);

/* Where the block sits when it is parked off the top of the screen. */
#define FORM_AWAY (-(float)(FORM_Y + FORM_ROWS * FORM_PITCH + 24))

/* Slides the parked block towards `target`, and returns true once it is there.
   Call it every tick for as long as it returns false. */
bool wave_lift(Wave *w, float target);

void wave_hold_entries(Wave *w, bool hold);
int  wave_divers(const Wave *w);
void wave_print_stats(const Wave *w);

/* Debug: builds each attack curve and prints its sampled polyline. */
void wave_dump_dives(void);

/* Every enemy has been shot. */
bool wave_cleared(const Wave *w);

/* --------------------------------------------------------------- capture */

/* True while `at` is inside an open tractor beam. `boss` receives the enemy
   holding it open. The wave owns the geometry, so the game does not have to
   know how a beam is shaped in order to be caught by one. */
bool wave_beam_catch(const Wave *w, Vec2 at, int *boss);

/* Hands a captured fighter to a boss, once the game has finished drawing it up
   the beam. From here the boss carries it home and flies with it. */
void wave_attach_captive(Wave *w, int boss);

/* Which enemy is carrying a captive, or -1. Killing that enemy frees it, which
   the game spots by watching this across a hit. */
int wave_captive_holder(const Wave *w);

/* Where a captive rides, so the game can fly the rescue from the right place. */
Vec2 wave_captive_pos(const Wave *w);

/* A shot at `at` hits the captured fighter rather than its captor. Destroys it
   and returns true, in which case the fighter is gone for good - the captive is
   a target in its own right, and it is the one in front. */
bool wave_captive_hit(Wave *w, Vec2 at);

/* Where an enemy currently is. The capture animation has to fly the fighter up
   to a boss that is still moving. */
Vec2 wave_enemy_pos(const Wave *w, int index);

/* Sends every diver home and clears the air. Called when the player dies: the
   arcade empties the screen rather than dropping a fresh ship into traffic,
   and without it you could respawn directly on top of an enemy mid-dive and
   lose the next life immediately. */
void wave_recall(Wave *w);

/* While paused no new attack launches. Anything already flying carries on, so
   the screen drains rather than freezing. */
void wave_pause_attacks(Wave *w, bool paused);

/* True when nothing that could kill the player is within `radius` of a point -
   used to hold a respawn until the spot is actually clear. */
bool wave_area_clear(const Wave *w, Vec2 at, float radius);

/* Drops every missile still in the air. Called when a stage ends: the enemies
   that fired them are gone, and a bullet left hanging would freeze in place
   through the pause that follows, since the wave stops being updated. */
void wave_clear_shots(Wave *w);

/* Registers a hit. Returns true if that killed it, false if it only took
   damage - which only a Boss Galaga does, switching to its damaged palette on
   the way. `score` receives the points earned, and `popup` is set to the same
   value when the kill is one the arcade puts on screen, or 0 when it is not. */
bool wave_hit(Wave *w, int index, int *score, int *popup);

/* How close something has to be to an enemy's centre to count as a hit. The
   art inside a 16x16 cell is smaller than the cell, so this is tighter than
   half a sprite. */
#define ENEMY_HIT_RADIUS 7.0f

/* Where a formation slot sits on screen. */
Vec2 formation_slot_pos(int slot);

#endif /* CLAUDAGA_FORMATION_H */
