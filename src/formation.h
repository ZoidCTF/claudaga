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

    /* Missiles the divers have fired. They outlive the enemy that fired them,
       so they belong to the wave rather than to an enemy. */
    EnemyShot shot[MAX_ENEMY_SHOTS];

    int   tick;
    int   next_attack;       /* tick the next attack is due */

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

    /* Closest two enemies have come while flying the same path. Enemies
       finishing a dive together used to re-enter stacked, so this is the
       number that says whether the lane queue is doing its job. */
    float min_lane_gap;

    /* Steepest missile fired, in degrees away from straight down. A shot that
       approaches 90 travels along the fighter's own row, which cannot be
       dodged, so this is the number that says the aim cone is holding. */
    float shot_max_deg;
    bool  show_paths;
    bool  attacks_enabled;
} Wave;

void wave_init(Wave *w);       /* builds paths and slots; call once */
void wave_restart(Wave *w);    /* sends everyone back off-screen */
void wave_update(Wave *w, float player_x);
void wave_draw(Gfx *g, const Wave *w);

bool wave_all_formed(const Wave *w);
int  wave_divers(const Wave *w);
void wave_print_stats(const Wave *w);

/* Every enemy has been shot. */
bool wave_cleared(const Wave *w);

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
