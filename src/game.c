#include "game.h"
#include "audio.h"
#include "font.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const SDL_Color WHITE  = { 255, 255, 255, 255 };
static const SDL_Color YELLOW = { 255, 216,   0, 255 };
static const SDL_Color CYAN   = {   0, 224, 255, 255 };
static const SDL_Color RED    = { 255,  72,  72, 255 };
static const SDL_Color DIM    = { 150, 150, 168, 255 };

#define PLAYER_SPEED    1.6f
#define SHOT_SPEED      4.0f
#define FIRE_COOLDOWN   12
#define RESPAWN_TICKS   70

/* How much room the fighter needs before it will come back. The recall sends
   the divers home, but one already close to the bottom takes a moment to leave,
   and reappearing inside it costs the next life straight away. */
#define SPAWN_CLEAR_RADIUS 34.0f

#define CAPTURE_LIFT   1.7f    /* how fast a caught fighter is drawn up   */
#define CAPTURE_SPIN   9.0f    /* degrees per tick while it is being taken */
#define RESCUE_SPEED   2.1f    /* a freed fighter's descent                */
#define STAGE_PAUSE     120
#define GAME_OVER_TICKS 240

/* How long the results screen holds before the game reports itself finished.
   Long enough to read four lines without hurrying; fire cuts it short. */
#define RESULTS_TICKS 420

/* How fast the surviving fighter walks to the middle during a reunion. Slower
   than steering, so it reads as the game taking over rather than as input. */
#define RESCUE_CENTRE_SPEED 1.4f

/* How long the extra-fighter notice stays up. */
#define EXTRA_MSG_TICKS 120

/* Extra fighters, on the arcade's default schedule: the first at 20,000, the
   second at 70,000, and one more every 70,000 after that. The spare-ship row
   only has space for five, so beyond that the count keeps rising but the
   display does not. */
#define FIRST_EXTRA_LIFE 20000
#define EXTRA_LIFE_EVERY 70000

/* Collision is a distance test rather than a box. The art inside a 16x16 cell
   is smaller than the cell, so these are all tighter than half a sprite - the
   arcade is famously forgiving about near misses and matching that matters
   more than being geometrically exact. */
#define R_SHOT_ENEMY    (ENEMY_HIT_RADIUS + 2.0f)
#define R_MISSILE_SHIP  8.0f
#define R_ENEMY_SHIP    9.0f

/* ------------------------------------------------------------------ stars */

#define STAR_COUNT 96

typedef struct {
    float     x, y, speed;
    int       phase;
    bool      lit;
    SDL_Color color;
} Star;

static Star s_stars[STAR_COUNT];

static void stars_init(void)
{
    static const SDL_Color palette[] = {
        { 255, 255, 255, 255 }, { 255,  96,  96, 255 },
        { 128, 192, 255, 255 }, { 255, 255, 128, 255 },
        { 128, 255, 192, 255 },
    };
    for (int i = 0; i < STAR_COUNT; ++i) {
        Star *s = &s_stars[i];
        s->x     = (float)(rand() % GAME_W);
        s->y     = (float)(rand() % GAME_H);
        s->speed = 0.25f + (float)(rand() % 100) / 100.0f * 0.75f;
        s->phase = 20 + rand() % 120;
        s->lit   = (rand() & 1) != 0;
        s->color = palette[rand() % ARRAY_COUNT(palette)];
    }
}

static void stars_update(void)
{
    for (int i = 0; i < STAR_COUNT; ++i) {
        Star *s = &s_stars[i];
        s->y += s->speed;
        if (s->y >= GAME_H) {
            s->y = 0.0f;
            s->x = (float)(rand() % GAME_W);
        }
        if (--s->phase <= 0) {
            s->lit   = !s->lit;
            s->phase = 20 + rand() % 120;
        }
    }
}

static void stars_draw(Gfx *g)
{
    for (int i = 0; i < STAR_COUNT; ++i) {
        const Star *s = &s_stars[i];
        if (!s->lit) continue;
        SDL_SetRenderDrawColor(g->renderer, s->color.r, s->color.g, s->color.b, 255);
        SDL_Rect px = { (int)s->x, (int)s->y, 1, 1 };
        SDL_RenderFillRect(g->renderer, &px);
    }
}

void game_background_update(void) { stars_update(); }
void game_background_draw(Gfx *gfx) { stars_draw(gfx); }

/* ------------------------------------------------------------------ setup */

static void clear_shots(Game *g);
static void start_stage(Game *g);

/* Reports what a freshly handed-out wave looks like: any boss already showing
   damage, or any shot still live, means something leaked across the stage
   boundary. */
static void trace_new_wave(const Game *g)
{
    int damaged = 0, live_shots = 0;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const Enemy *e = &g->wave.enemies[i];
        if (e->hits > 0) ++damaged;
    }
    for (int i = 0; i < MAX_SHOTS; ++i) if (g->shots[i].alive) ++live_shots;
    int enemy_shots = 0;
    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) {
        if (g->wave.shot[i].alive) ++enemy_shots;
    }
    printf("tick %d: stage %d starts - damaged bosses %d, stray shots %d, "
           "stray missiles %d\n",
           g->tick, g->stage, damaged, live_shots, enemy_shots);
}

/* The high score outlives the process, and is the only file this game touches.
   It goes wherever SDL says user data belongs rather than next to the
   executable, which is often somewhere unwritable - and every failure here is
   silent on purpose: a game that will not start because it could not read a
   high score would be a worse game than one that forgets. */
static void highscore_path(char *out, size_t n)
{
    char *pref = SDL_GetPrefPath("Claudaga", "Claudaga");
    if (!pref) { out[0] = 0; return; }
    snprintf(out, n, "%shighscore", pref);
    SDL_free(pref);
}

static int highscore_load(void)
{
    char path[512];
    highscore_path(path, sizeof path);
    if (!path[0]) return 0;

    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int v = 0;
    if (fscanf(f, "%d", &v) != 1) v = 0;
    fclose(f);
    return v > 0 ? v : 0;
}

static void highscore_save(int v)
{
    char path[512];
    highscore_path(path, sizeof path);
    if (!path[0]) return;

    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d\n", v);
    fclose(f);
}

void game_init(Game *g)
{
    memset(g, 0, sizeof *g);
    g->first_stage  = 1;
    g->other_score  = -1;
    wave_init(&g->wave);
    stars_init();

    g->quiet = true;    /* this one is scenery for the menu, not a stage */
    game_restart(g);
    g->quiet = false;

    /* After game_restart rather than before: a restart is a new game, not a
       new machine, so it leaves the high score alone - which means this is the
       one place it can be set. */
    g->high_score = highscore_load();
}

void game_restart(Game *g)
{
    g->score       = 0;
    g->stage       = g->first_stage > 0 ? g->first_stage : 1;
    g->tick        = 0;
    g->stage_clear = 0;
    g->game_over   = 0;
    g->last_death_tick = 0;

    g->player.x        = GAME_W / 2.0f;
    g->player.alive    = true;
    g->player.dual     = false;
    g->player.lives    = START_LIVES;
    g->player.respawn  = 0;
    g->player.captured = false;
    g->player.cap_boss = -1;
    g->rescue_active   = false;

    clear_shots(g);
    g->fire_cooldown = 0;

    g->bonus_hits  = 0;
    g->bonus_award = 0;
    g->finished    = false;
    g->turn_over   = false;

    g->next_life      = FIRST_EXTRA_LIFE;
    g->extra_msg      = 0;
    g->shots_fired    = 0;
    g->shots_hit      = 0;
    g->results        = 0;
    g->results_armed  = false;

    fx_reset(&g->fx);
    start_stage(g);
}

/* Every fourth stage from the third is a bonus round, as on the real board:
   3, 7, 11 and so on. */
static bool is_challenge_stage(int stage)
{
    return stage >= 3 && ((stage - 3) % 4) == 0;
}

/* How many challenging stages come before this one. Used to count the ordinary
   stages, which is what picks the entry the wave flies in on - a bonus round
   flies no entry, so counting it would waste a set. */
static int challenges_before(int stage)
{
    return stage <= 3 ? 0 : (stage - 4) / 4 + 1;
}

/* Hands out whichever kind of wave the stage number calls for. */
/* Hands out whichever kind of wave the stage number calls for.

   Silent while the game is merely being set up. game_init builds a whole game
   behind the title screen so the menu has something to draw a starfield over,
   and that used to announce itself: the round-start jingle played over the
   menu appearing, every launch, for a stage nobody was about to play. */
static void start_stage(Game *g)
{
    if (is_challenge_stage(g->stage)) {
        wave_restart_challenge(&g->wave, g->stage, (g->stage - 3) / 4);
        if (!g->quiet) audio_music(MUSIC_BONUS);
    } else {
        wave_restart(&g->wave, g->stage,
                     g->stage - 1 - challenges_before(g->stage));
        if (!g->quiet) audio_music_stop();
    }
    if (!g->quiet) audio_play(SFX_STAGE);
}

/* ------------------------------------------------------------- collision */

static float dist2(Vec2 a, Vec2 b)
{
    float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static Vec2 player_pos(const Game *g)
{
    Vec2 p = { g->player.x, (float)PLAYER_Y };
    return p;
}

/* Where each hull sits. A single fighter is one at the centre; a dual is two
   either side of it. Everything that needs to hit or be hit works through
   this, so the pair is genuinely two targets rather than one wide one. */
static int player_hulls(const Game *g, Vec2 *out)
{
    if (!g->player.dual) {
        out[0] = player_pos(g);
        return 1;
    }
    out[0].x = g->player.x - DUAL_OFFSET; out[0].y = (float)PLAYER_Y;
    out[1].x = g->player.x + DUAL_OFFSET; out[1].y = (float)PLAYER_Y;
    return 2;
}

/* A dual fighter that takes a hit loses the rescued hull and flies on as a
   single. That is the point of rescuing one: it buys a hit, not a life. */
static bool lose_wingman(Game *g, Vec2 at)
{
    if (!g->player.dual) return false;
    g->player.dual = false;
    fx_blast_player(&g->fx, at);
    return true;
}

static void start_capture(Game *g, int boss)
{
    if (g->player.captured) return;

    g->player.captured = true;
    g->player.alive    = false;
    g->gone_tick       = g->tick;
    g->turn_over       = true;
    g->player.cap_pos  = player_pos(g);
    g->player.cap_spin = 0.0f;
    g->player.cap_boss = boss;

    if (g->trace) printf("tick %d: fighter captured by enemy %d\n", g->tick, boss);

    /* A capture costs a fighter exactly as a death does, but there is no wreck
       and the wave is not cleared - the boss is in the middle of something.

       --observe still lets the capture happen and only skips the cost. Blocking
       it outright meant the whole capture-and-rescue chain could not be watched
       headlessly at all, and it is the most involved thing the game does. */
    if (!g->invulnerable && --g->player.lives <= 0) g->game_over = GAME_OVER_TICKS;
}

static void kill_player(Game *g)
{
    if (!g->player.alive || g->invulnerable) return;
    g->player.alive   = false;
    g->player.respawn = RESPAWN_TICKS;
    g->gone_tick      = g->tick;
    g->turn_over      = true;
    fx_blast_player(&g->fx, player_pos(g));
    audio_play(SFX_PLAYER_DIE);

    if (g->last_death_tick > 0) {
        int gap = g->tick - g->last_death_tick;
        if (g->min_death_gap == 0 || gap < g->min_death_gap) g->min_death_gap = gap;
        if (g->trace) {
            printf("tick %d: player died, %d ticks after the last one\n",
                   g->tick, gap);
        }
    }
    g->last_death_tick = g->tick;

    /* Empty the screen. Everything mid-dive is sent home and the missiles in
       the air are dropped, so the replacement ship arrives to a clear board
       rather than into the middle of the attack that just killed it. */
    wave_recall(&g->wave);

    if (--g->player.lives <= 0) g->game_over = GAME_OVER_TICKS;
}

/* Player shots against the wave. A shot is spent on the first thing it
   touches, including a boss that survives the hit. */
/* Everything that adds to the score goes through here, so the extra-fighter
   check and the running high score cannot be forgotten at a new call site.
   The award loops rather than testing once: a perfect bonus round pays 10,000
   in a single go and could otherwise step straight over a threshold. */
static void add_score(Game *g, int n)
{
    g->score += n;

    while (g->score >= g->next_life) {
        ++g->player.lives;
        g->next_life = (g->next_life == FIRST_EXTRA_LIFE)
                     ? EXTRA_LIFE_EVERY
                     : g->next_life + EXTRA_LIFE_EVERY;
        g->extra_msg = EXTRA_MSG_TICKS;
        audio_play(SFX_EXTRA);
        if (g->trace) {
            printf("tick %d: extra fighter at %d, now %d in reserve, next at %d\n",
                   g->tick, g->score, g->player.lives - 1, g->next_life);
        }
    }

    /* A demo plays for real but does not count. */
    if (!g->demo && g->score > g->high_score) g->high_score = g->score;
}

static void collide_shots(Game *g)
{
    for (int i = 0; i < MAX_SHOTS; ++i) {
        Shot *sh = &g->shots[i];
        if (!sh->alive) continue;

        /* The captured fighter is a target in its own right. Destroying it
           costs the fighter for good and pays nothing, because it is your own
           ship.

           It rides above its captor and the two never overlap - fifteen pixels
           apart against a seven pixel hit radius - so a shot can only ever be
           in range of one of them, and testing this first costs nothing and
           decides nothing. The boss is the one nearer the player, so a shot
           straight up the column reaches the boss and frees the fighter; the
           captive is what you hit when the pair is not lined up with you. */
        if (wave_captive_hit(&g->wave, sh->pos)) {
            sh->alive = false;
            ++g->shots_hit;
            fx_blast_enemy(&g->fx, sh->pos);
            audio_play(SFX_ENEMY_DIE);
            if (g->trace) {
                printf("tick %d: captured fighter shot down - it is gone\n",
                       g->tick);
            }
            continue;
        }

        for (int e = 0; e < MAX_ENEMIES; ++e) {
            const Enemy *en = &g->wave.enemies[e];
            if (en->state == ENEMY_DEAD || en->state == ENEMY_WAITING) continue;
            if (dist2(sh->pos, en->pos) > R_SHOT_ENEMY * R_SHOT_ENEMY) continue;

            Vec2 at = en->pos;
            int  score = 0, popup = 0;
            int  held_before = wave_captive_holder(&g->wave);
            bool killed = wave_hit(&g->wave, e, &score, &popup);

            sh->alive = false;
            ++g->shots_hit;   /* a boss that survives the hit still counts */
            audio_play(killed ? SFX_ENEMY_DIE : SFX_BOSS_HIT);
            if (killed) {
                add_score(g, score);
                fx_blast_enemy(&g->fx, at);
                if (popup > 0) fx_score(&g->fx, at, popup);

                /* That was the boss carrying the captured fighter, so it comes
                   back. It flies down from where its captor died. */
                if (held_before == e && wave_captive_holder(&g->wave) < 0
                    && !g->player.dual) {
                    g->rescue_active = true;
                    g->rescue_pos    = at;

                    /* The arcade plays the reunion out rather than letting it
                       happen in traffic: the board is sent home, no new attack
                       launches, and the fighter is walked to the middle to meet
                       the one coming down. Steering during it would let the
                       player pull away from the very thing flying to meet
                       them. Control comes back when they dock. */
                    wave_recall(&g->wave);

                    if (g->trace) printf("tick %d: captor destroyed, fighter freed\n",
                                         g->tick);
                }
            }
            break;
        }
    }
}

/* The wave against the fighter: missiles, and the divers themselves. */
static void collide_player(Game *g)
{
    if (!g->player.alive) return;

    Vec2 hull[2];
    int  hulls = player_hulls(g, hull);

    /* A beam takes the fighter rather than destroying it, and it is checked
       first: being caught while an enemy happens to be overhead should read as
       a capture, which is recoverable, not as a death. */
    int boss = -1;
    for (int h = 0; h < hulls; ++h) {
        if (wave_beam_catch(&g->wave, hull[h], &boss)) {
            if (!lose_wingman(g, hull[h])) start_capture(g, boss);
            return;
        }
    }

    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) {
        EnemyShot *s = &g->wave.shot[i];
        if (!s->alive) continue;
        for (int h = 0; h < hulls; ++h) {
            if (dist2(s->pos, hull[h]) <= R_MISSILE_SHIP * R_MISSILE_SHIP) {
                s->alive = false;
                if (!lose_wingman(g, hull[h])) kill_player(g);
                return;
            }
        }
    }

    /* Nothing in a bonus round can hurt the fighter - the flyers are there to
       be shot at, not to fight back. */
    if (wave_is_challenge(&g->wave)) return;

    /* Only a diver can reach the fighter, but testing every live enemy costs
       nothing and means a stray one cannot pass through it unnoticed. */
    for (int e = 0; e < MAX_ENEMIES; ++e) {
        const Enemy *en = &g->wave.enemies[e];
        if (en->state == ENEMY_DEAD || en->state == ENEMY_WAITING) continue;
        for (int h = 0; h < hulls; ++h) {
            if (dist2(en->pos, hull[h]) <= R_ENEMY_SHIP * R_ENEMY_SHIP) {
                if (!lose_wingman(g, hull[h])) kill_player(g);
                return;
            }
        }
    }
}

/* ----------------------------------------------------------------- update */

static void advance_shots(Game *g)
{
    for (int i = 0; i < MAX_SHOTS; ++i) {
        Shot *sh = &g->shots[i];
        if (!sh->alive) continue;
        sh->pos.y -= SHOT_SPEED;
        if (sh->pos.y < -CELL) sh->alive = false;
    }
}

static void clear_shots(Game *g)
{
    for (int i = 0; i < MAX_SHOTS; ++i) g->shots[i].alive = false;
}

/* Movement, on its own, because the fighter keeps flying during the pause
   between stages. The arcade never takes the controls away there, and having
   it lock up mid-screen while the message is on reads as the game hanging. */
static void steer_player(Game *g, const Input *in)
{
    if (in->left)  g->player.x -= PLAYER_SPEED;
    if (in->right) g->player.x += PLAYER_SPEED;

    /* A dual fighter is twice as wide, so it stops sooner at the edges. */
    float half = CELL / 2.0f + (g->player.dual ? DUAL_OFFSET : 0.0f);
    if (g->player.x < half)          g->player.x = half;
    if (g->player.x > GAME_W - half) g->player.x = GAME_W - half;
}

static int shots_in_air(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_SHOTS; ++i) if (g->shots[i].alive) ++n;
    return n;
}

static void spawn_shot(Game *g, float x)
{
    for (int i = 0; i < MAX_SHOTS; ++i) {
        if (g->shots[i].alive) continue;
        g->shots[i].alive = true;
        g->shots[i].pos.x = x;
        g->shots[i].pos.y = (float)(PLAYER_Y - 8);
        ++g->shots_fired;
        return;
    }
}

static void fire(Game *g)
{
    Vec2 hull[2];
    int  hulls = player_hulls(g, hull);
    int  cap   = g->player.dual ? SHOTS_DUAL : SHOTS_SINGLE;

    if (shots_in_air(g) + hulls > cap) return;

    for (int h = 0; h < hulls; ++h) spawn_shot(g, hull[h].x);
    g->fire_cooldown = FIRE_COOLDOWN;
    audio_play(SFX_SHOT);
}

/* A captured fighter being drawn up the beam. It follows the boss rather than
   a fixed point, so a captor that starts for home takes the fighter with it.

   This runs through the game-over message as well as through play. Losing the
   last fighter to a beam does not stop the beam: the arcade hoists it up to
   the boss while GAME OVER sits on screen, and leaving it hanging in mid-air
   instead reads as the game having crashed rather than ended. */
static void update_capture_lift(Game *g)
{
    Vec2 boss = wave_enemy_pos(&g->wave, g->player.cap_boss);
    float dx = boss.x - g->player.cap_pos.x;
    float dy = boss.y - g->player.cap_pos.y;
    float d  = sqrtf(dx * dx + dy * dy);

    g->player.cap_spin += CAPTURE_SPIN;
    if (d <= CAPTURE_LIFT) {
        wave_attach_captive(&g->wave, g->player.cap_boss);
        g->player.captured = false;
        g->player.respawn  = RESPAWN_TICKS;
    } else {
        g->player.cap_pos.x += dx / d * CAPTURE_LIFT;
        g->player.cap_pos.y += dy / d * CAPTURE_LIFT;
    }
}

/* Slid to the middle rather than snapped: the rescued fighter is flying down to
   a fixed point, and a ship that jumped there would make the two arrive by
   different rules. */
static void walk_to_centre(Game *g)
{
    float dx = GAME_W / 2.0f - g->player.x;
    if (dx > RESCUE_CENTRE_SPEED)       g->player.x += RESCUE_CENTRE_SPEED;
    else if (dx < -RESCUE_CENTRE_SPEED) g->player.x -= RESCUE_CENTRE_SPEED;
    else                                g->player.x  = GAME_W / 2.0f;
}

/* A freed fighter flying down to dock. The target is the middle of the
   fighter's row rather than wherever the player happens to be, since the
   player is being walked to that same spot. If there is no ship there yet,
   because the capture cost the last one, it keeps station until one comes
   back.

   Like the lift above, this has to keep running when the rest of the game has
   stopped - the stage-clear pause is exactly when a reunion is most likely to
   be under way, because the captor was often the last enemy on the board. */
static void update_rescue(Game *g)
{
    if (!g->rescue_active) return;

    float tx = GAME_W / 2.0f, ty = (float)PLAYER_Y;
    float dx = tx - g->rescue_pos.x, dy = ty - g->rescue_pos.y;
    float d  = sqrtf(dx * dx + dy * dy);

    if (d <= RESCUE_SPEED) {
        g->rescue_pos.x = tx;
        g->rescue_pos.y = ty;

        /* Both halves have to be in place. The rescued fighter arrives first
           as often as not, and docking with a ship still sliding across would
           snap it the rest of the way. */
        if (g->player.alive && g->player.x == tx) {
            g->player.dual   = true;
            g->rescue_active = false;
            audio_play(SFX_EXTRA);
            if (g->trace) printf("tick %d: dual fighter docked\n", g->tick);
        }
    } else {
        g->rescue_pos.x += dx / d * RESCUE_SPEED;
        g->rescue_pos.y += dy / d * RESCUE_SPEED;
    }
}

void game_update(Game *g, const Input *in)
{
    ++g->tick;
    stars_update();
    fx_update(&g->fx);
    if (g->extra_msg > 0) --g->extra_msg;

    /* Shots fly regardless of what else the game is doing. This has to sit
       above the early returns below: when it lived after them, a shot in the
       air as the last enemy died simply stopped, hung on screen for the whole
       between-stage pause, and then carried into the next wave still live -
       where it promptly hit whatever flew past it, which is what made fresh
       Boss Galagas turn up already damaged. */
    advance_shots(g);

    /* Once the crew is gone the wave keeps flying behind the message. When it
       has been up long enough the game reports that it is finished rather than
       quietly starting itself over - being dropped back into a fresh stage 1
       with no acknowledgement reads as a glitch. */
    if (g->game_over > 0) {
        wave_update(&g->wave, g->player.x);

        /* A fighter taken by the last beam is still carried off. The wave is
           still flying behind the message, so the boss it is being drawn
           towards is still moving, and the two finish the journey together. */
        if (g->player.captured) update_capture_lift(g);

        if (--g->game_over == 0) {
            g->results       = RESULTS_TICKS;
            g->results_armed = false;
            if (!g->demo) highscore_save(g->high_score);
        }
        return;
    }

    /* The results screen. The board is frozen behind it rather than still
       flying: the game is over, and a wave carrying on underneath the numbers
       reads as though it were not.

       Fire cuts it short, but only once it has been let go of first. Without
       that, holding the trigger as the last fighter dies - which is exactly
       what a player is doing at that moment - would skip the screen before it
       had drawn a single frame. */
    if (g->results > 0) {
        bool firing = in && in->fire;
        if (!firing) g->results_armed = true;
        else if (g->results_armed) g->results = 1;

        if (--g->results == 0) {
            g->finished = true;
            if (g->trace) {
                printf("tick %d: game over, score %d, stage %d, %d of %d shots hit\n",
                       g->tick, g->score, g->stage, g->shots_hit, g->shots_fired);
            }
        }
        return;
    }

    if (g->finished) return;   /* held until someone restarts or leaves */

    /* Between stages: hold briefly on an empty screen, then send in the next
       wave. Nothing else about a stage changes yet - see the README. */
    if (g->stage_clear > 0) {
        /* Still your ship: it flies and it shoots. Anything fired here is
           cleared when the next wave is handed out, so nothing leaks across
           the boundary. */
        if (g->player.alive) {
            if (g->rescue_active) {
                walk_to_centre(g);
            } else {
                steer_player(g, in);
                if (g->fire_cooldown > 0) --g->fire_cooldown;
                if (in->fire && g->fire_cooldown == 0) fire(g);
            }
        }
        collide_shots(g);
        update_rescue(g);

        /* The pause holds while a rescued fighter is still on its way down.
           A captor is very often the last enemy on the board, so the stage
           clears at the very moment the reunion starts - and the next wave
           used to arrive on top of a fighter still flying to its dock. */
        if (g->rescue_active) return;

        if (--g->stage_clear == 0) {
            ++g->stage;
            clear_shots(g);        /* nothing from the old wave survives */
            start_stage(g);
            if (g->trace) trace_new_wave(g);
        }
        return;
    }

    /* No new attacks while there is nobody to attack, and none through the
       reunion either - the player cannot steer during it, so anything launched
       would be shooting at a target that has been taken away from them. */
    wave_pause_attacks(&g->wave, !g->player.alive || g->rescue_active);

    wave_update(&g->wave, g->player.x);

    if (g->player.captured) {
        update_capture_lift(g);
    } else if (g->player.alive) {
        if (g->rescue_active) {
            walk_to_centre(g);
        } else {
            steer_player(g, in);

            if (g->fire_cooldown > 0) --g->fire_cooldown;
            if (in->fire && g->fire_cooldown == 0) fire(g);
        }
    } else {
        /* Respawn once the countdown has run out, the explosion has finished
           playing, and the spot is actually clear. The first keeps a fresh ship
           from appearing inside the wreck of the old one; the last keeps it
           from appearing inside whatever is still on its way off the screen. */
        if (g->player.respawn > 0) --g->player.respawn;

        Vec2 spot = { GAME_W / 2.0f, (float)PLAYER_Y };
        if (g->player.respawn == 0 && !g->turn_over
            && !fx_player_blast_active(&g->fx)
            && wave_area_clear(&g->wave, spot, SPAWN_CLEAR_RADIUS)) {
            g->player.alive = true;
            g->player.x     = spot.x;
            if (g->trace) {
                printf("tick %d: fighter back on the line, %d ticks after it went\n",
                       g->tick, g->tick - g->gone_tick);
            }
        }
    }

    update_rescue(g);

    collide_shots(g);
    collide_player(g);

    if (wave_cleared(&g->wave) && g->stage_clear == 0) {
        g->stage_clear = STAGE_PAUSE;

        /* A bonus round pays out when it ends: a flat rate per flyer caught on
           the way through, and a much larger bonus for catching every one. */
        if (wave_is_challenge(&g->wave)) {
            g->bonus_hits  = wave_challenge_hits(&g->wave);
            g->bonus_award = (g->bonus_hits >= MAX_ENEMIES) ? CHALLENGE_PERFECT : 0;
            add_score(g, g->bonus_award);

            /* The round is over, so its music goes rather than running on
               under the payout. */
            audio_music_stop();
            if (g->bonus_award > 0) audio_play(SFX_PERFECT);
            if (g->trace) {
                printf("tick %d: bonus round over - %d of %d caught, bonus %d\n",
                       g->tick, g->bonus_hits, MAX_ENEMIES, g->bonus_award);
            }
        } else {
            g->bonus_hits = g->bonus_award = 0;
        }

        /* The stage is over, so the screen empties. Enemy missiles especially
           have to go now rather than at the end of the pause: the wave stops
           being updated while the message is up, so anything still in the air
           would hang motionless on screen for the whole two seconds. Dropping
           them also avoids a bullet from an already-dead enemy taking a life
           during the congratulations. */
        if (g->trace) {
            int m = 0;
            for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) {
                if (g->wave.shot[i].alive) ++m;
            }
            int p = 0;
            for (int i = 0; i < MAX_SHOTS; ++i) if (g->shots[i].alive) ++p;
            printf("tick %d: stage %d cleared - dropping %d missile(s) and "
                   "%d shot(s) still in the air\n", g->tick, g->stage, m, p);
        }

        clear_shots(g);
        wave_clear_shots(&g->wave);
    }
}

/* ------------------------------------------------------------------- draw */

/* Galaga's end-of-game screen: how much was fired, how much of it landed, and
   the ratio between them. Drawn on an empty sky rather than over the board,
   because the game is over and the numbers are the whole point of the screen.
   The ratio is what the arcade actually put up, and it is a more honest
   summary of a run than the score - a player who reached stage 8 by spraying
   and one who reached it by aiming score much the same. */
static void draw_results(Gfx *gfx, const Game *g)
{
    char buf[40];
    int  y = 84;

    const char *title = "-RESULTS-";
    font_draw_scaled(gfx, (GAME_W - font_width_scaled(title, 2.0f)) / 2,
                     (float)(y - 30), YELLOW, title, 2.0f);

    /* Label left, value right, on a pair of columns rather than centred: the
       three numbers line up under one another and can be compared at a
       glance, which centring each line separately would lose. */
    const int LX = 30, RX = GAME_W - 30;

    snprintf(buf, sizeof buf, "%d", g->shots_fired);
    font_draw(gfx, LX, y, WHITE, "SHOTS FIRED");
    font_draw(gfx, RX - font_width(buf), y, CYAN, buf);
    y += 16;

    snprintf(buf, sizeof buf, "%d", g->shots_hit);
    font_draw(gfx, LX, y, WHITE, "NUMBER OF HITS");
    font_draw(gfx, RX - font_width(buf), y, CYAN, buf);
    y += 16;

    /* Fired can be zero - a fighter can be lost without ever shooting - and a
       ratio of nothing out of nothing is 0, not a divide by zero. */
    int pct10 = g->shots_fired > 0
              ? (g->shots_hit * 1000 + g->shots_fired / 2) / g->shots_fired
              : 0;
    snprintf(buf, sizeof buf, "%d.%d %%", pct10 / 10, pct10 % 10);
    font_draw(gfx, LX, y, WHITE, "HIT-MISS RATIO");
    font_draw(gfx, RX - font_width(buf), y, YELLOW, buf);

    snprintf(buf, sizeof buf, "SCORE %d", g->score);
    font_draw(gfx, (GAME_W - font_width(buf)) / 2, y + 40, WHITE, buf);

    if (g->score >= g->high_score && g->score > 0) {
        const char *best = "NEW HIGH SCORE";
        font_draw(gfx, (GAME_W - font_width(best)) / 2, y + 54, YELLOW, best);
    }
}

void game_draw(Gfx *gfx, const Game *g)
{
    stars_draw(gfx);

    if (g->results > 0) { draw_results(gfx, g); return; }

    wave_draw(gfx, &g->wave);

    for (int i = 0; i < MAX_SHOTS; ++i) {
        const Shot *sh = &g->shots[i];
        if (!sh->alive) continue;
        shape_draw(gfx, SHP_PLAYER_SHOT, sh->pos, HEADING_N, 1.0f);
    }

    if (g->player.alive) {
        Vec2 hull[2];
        int  hulls = player_hulls(g, hull);
        for (int h = 0; h < hulls; ++h) {
            shape_draw(gfx, SHP_FIGHTER, hull[h], HEADING_N, 1.0f);
        }
    }

    /* Being drawn up a beam: still the player's ship, tumbling. */
    if (g->player.captured) {
        shape_draw(gfx, SHP_FIGHTER, g->player.cap_pos, g->player.cap_spin, 1.0f);
    }

    /* A freed fighter on its way down to dock. */
    if (g->rescue_active) {
        shape_draw(gfx, SHP_FIGHTER, g->rescue_pos, HEADING_N, 1.0f);
    }

    fx_draw(gfx, &g->fx);

    /* HUD: this seat's score on the left, the machine's best in the middle,
       and on the right either the stage or - with two playing - the other
       seat's score. There is no room for both, and the stage is already
       spelled out in flags along the bottom, so the score is the one that
       earns the space.

       The label of whichever seat is playing blinks, which is how the arcade
       says whose turn it is without a word of explanation. */
    char buf[32];
    bool two = (g->other_score >= 0);
    bool lit = ((g->tick / 20) % 2) == 0;

    const char *mine  = (g->seat == 0) ? "1UP" : "2UP";
    const char *their = (g->seat == 0) ? "2UP" : "1UP";

    font_draw(gfx, 4, 2, (two && !lit) ? DIM : WHITE, mine);
    snprintf(buf, sizeof buf, "%d", g->score);
    font_draw(gfx, 4, 10, YELLOW, buf);

    {
        const char *hs = "HIGH SCORE";
        font_draw(gfx, (GAME_W - font_width(hs)) / 2, 2, RED, hs);
        snprintf(buf, sizeof buf, "%d", g->high_score);
        font_draw(gfx, (GAME_W - font_width(buf)) / 2, 10, YELLOW, buf);
    }

    if (two) {
        font_draw(gfx, GAME_W - font_width(their) - 4, 2, DIM, their);
        snprintf(buf, sizeof buf, "%d", g->other_score);
        font_draw(gfx, GAME_W - font_width(buf) - 4, 10, DIM, buf);
    } else {
        if (wave_is_challenge(&g->wave)) {
            snprintf(buf, sizeof buf, "BONUS %d", g->stage);
        } else {
            snprintf(buf, sizeof buf, "STAGE %d", g->stage);
        }
        font_draw(gfx, GAME_W - font_width(buf) - 4, 2, CYAN, buf);
    }

    /* A bonus round announces itself, and shows the running tally, since the
       whole round is about how many are caught. */
    if (wave_is_challenge(&g->wave) && g->stage_clear == 0) {
        const char *m = "CHALLENGING STAGE";
        font_draw(gfx, (GAME_W - font_width(m)) / 2, 20, YELLOW, m);
        snprintf(buf, sizeof buf, "%d", wave_challenge_hits(&g->wave));
        font_draw(gfx, (GAME_W - font_width(buf)) / 2, 30, DIM, buf);
    }

    /* Spare lives along the bottom left, drawn as small copies of the ship
       itself rather than a separate icon - one more thing the vector artwork
       gets for nothing. */
    for (int i = 0; i < g->player.lives - 1 && i < 5; ++i) {
        Vec2 p = { 9.0f + i * 13.0f, (float)(GAME_H - 9) };
        shape_draw(gfx, SHP_FIGHTER, p, HEADING_N, 0.72f);
    }

    /* Stage flags along the bottom right, spelling the stage number out with
       the largest denominations first - 23 is a twenty and three ones. They
       are laid out from the right edge inwards, so the count grows leftwards
       the way the arcade's does, and are capped at what the row will hold. */
    {
        float fx_x = GAME_W - 8.0f;
        int   left = g->stage;
        int   drawn = 0;
        for (int k = 0; k < FLAG_KINDS && drawn < 12; ++k) {
            while (left >= SHAPE_FLAG_VALUE[k] && drawn < 12) {
                Vec2 p = { fx_x, (float)(GAME_H - 9) };
                shape_draw_pal(gfx, SHP_FLAG, p, HEADING_N, 0.62f,
                               &SHAPE_PAL_FLAG[k], 1.0f);
                left -= SHAPE_FLAG_VALUE[k];
                fx_x -= 8.0f;
                ++drawn;
            }
        }
    }

    /* The extra fighter announces itself low on the screen, beside the spare
       ships it just added to, rather than in the middle of the play area. On
       the arcade this is a jingle; there is no sound yet, so without something
       on screen the reward would happen silently. */
    if (g->extra_msg > 0) {
        const char *msg = "EXTRA FIGHTER";
        font_draw(gfx, (GAME_W - font_width(msg)) / 2, GAME_H - 24, YELLOW, msg);
    }

    if (g->game_over > 0) {
        const char *msg = "GAME OVER";
        font_draw(gfx, (GAME_W - font_width(msg)) / 2, GAME_H / 2 - 4, RED, msg);
    } else if (g->stage_clear > 0) {
        if (g->bonus_hits > 0 || wave_is_challenge(&g->wave)) {
            snprintf(buf, sizeof buf, "%d OF %d", g->bonus_hits, MAX_ENEMIES);
            font_draw(gfx, (GAME_W - font_width(buf)) / 2, GAME_H / 2 - 12, CYAN, buf);
            if (g->bonus_award > 0) {
                snprintf(buf, sizeof buf, "PERFECT %d", g->bonus_award);
                font_draw(gfx, (GAME_W - font_width(buf)) / 2, GAME_H / 2 + 2,
                          YELLOW, buf);
            }
        } else {
            const char *msg = "STAGE CLEAR";
            font_draw(gfx, (GAME_W - font_width(msg)) / 2, GAME_H / 2 - 4, CYAN, msg);
        }
    }
}
