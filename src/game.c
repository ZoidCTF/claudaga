#include "game.h"
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

void game_init(Game *g)
{
    memset(g, 0, sizeof *g);
    wave_init(&g->wave);
    stars_init();
    game_restart(g);
}

void game_restart(Game *g)
{
    g->score       = 0;
    g->stage       = 1;
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

    fx_reset(&g->fx);
    start_stage(g);
}

/* Every fourth stage from the third is a bonus round, as on the real board:
   3, 7, 11 and so on. */
static bool is_challenge_stage(int stage)
{
    return stage >= 3 && ((stage - 3) % 4) == 0;
}

/* Hands out whichever kind of wave the stage number calls for. */
static void start_stage(Game *g)
{
    if (is_challenge_stage(g->stage)) {
        wave_restart_challenge(&g->wave, g->stage / 4);
    } else {
        wave_restart(&g->wave);
    }
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
    fx_blast_player(&g->fx, player_pos(g));

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
static void collide_shots(Game *g)
{
    for (int i = 0; i < MAX_SHOTS; ++i) {
        Shot *sh = &g->shots[i];
        if (!sh->alive) continue;

        for (int e = 0; e < MAX_ENEMIES; ++e) {
            const Enemy *en = &g->wave.enemies[e];
            if (en->state == ENEMY_DEAD || en->state == ENEMY_WAITING) continue;
            if (dist2(sh->pos, en->pos) > R_SHOT_ENEMY * R_SHOT_ENEMY) continue;

            Vec2 at = en->pos;
            int  score = 0, popup = 0;
            int  held_before = wave_captive_holder(&g->wave);
            bool killed = wave_hit(&g->wave, e, &score, &popup);

            sh->alive = false;
            if (killed) {
                g->score += score;
                fx_blast_enemy(&g->fx, at);
                if (popup > 0) fx_score(&g->fx, at, popup);

                /* That was the boss carrying the captured fighter, so it comes
                   back. It flies down from where its captor died. */
                if (held_before == e && wave_captive_holder(&g->wave) < 0
                    && !g->player.dual) {
                    g->rescue_active = true;
                    g->rescue_pos    = at;
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
static void steer_player(Game *g, const Uint8 *keys)
{
    if (keys[SDL_SCANCODE_LEFT])  g->player.x -= PLAYER_SPEED;
    if (keys[SDL_SCANCODE_RIGHT]) g->player.x += PLAYER_SPEED;

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
}

void game_update(Game *g, const Uint8 *keys)
{
    ++g->tick;
    stars_update();
    fx_update(&g->fx);

    /* Shots fly regardless of what else the game is doing. This has to sit
       above the early returns below: when it lived after them, a shot in the
       air as the last enemy died simply stopped, hung on screen for the whole
       between-stage pause, and then carried into the next wave still live -
       where it promptly hit whatever flew past it, which is what made fresh
       Boss Galagas turn up already damaged. */
    advance_shots(g);

    /* Once the crew is gone the wave keeps flying behind the message, then the
       whole thing starts over. */
    if (g->game_over > 0) {
        wave_update(&g->wave, g->player.x);
        if (--g->game_over == 0) game_restart(g);
        return;
    }

    /* Between stages: hold briefly on an empty screen, then send in the next
       wave. Nothing else about a stage changes yet - see the README. */
    if (g->stage_clear > 0) {
        /* Still your ship: it flies and it shoots. Anything fired here is
           cleared when the next wave is handed out, so nothing leaks across
           the boundary. */
        if (g->player.alive) {
            steer_player(g, keys);
            if (g->fire_cooldown > 0) --g->fire_cooldown;
            if (keys[SDL_SCANCODE_SPACE] && g->fire_cooldown == 0) fire(g);
        }
        collide_shots(g);

        if (--g->stage_clear == 0) {
            ++g->stage;
            clear_shots(g);        /* nothing from the old wave survives */
            start_stage(g);
            if (g->trace) trace_new_wave(g);
        }
        return;
    }

    /* No new attacks while there is nobody to attack. */
    wave_pause_attacks(&g->wave, !g->player.alive);

    wave_update(&g->wave, g->player.x);

    if (g->player.captured) {
        /* Drawn up the beam, turning over as it goes. It follows the boss
           rather than a fixed point, so a captor that starts for home takes
           the fighter with it. */
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
    } else if (g->player.alive) {
        steer_player(g, keys);

        if (g->fire_cooldown > 0) --g->fire_cooldown;
        if (keys[SDL_SCANCODE_SPACE] && g->fire_cooldown == 0) fire(g);
    } else {
        /* Respawn once the countdown has run out, the explosion has finished
           playing, and the spot is actually clear. The first keeps a fresh ship
           from appearing inside the wreck of the old one; the last keeps it
           from appearing inside whatever is still on its way off the screen. */
        if (g->player.respawn > 0) --g->player.respawn;

        Vec2 spot = { GAME_W / 2.0f, (float)PLAYER_Y };
        if (g->player.respawn == 0 && !fx_player_blast_active(&g->fx)
            && wave_area_clear(&g->wave, spot, SPAWN_CLEAR_RADIUS)) {
            g->player.alive = true;
            g->player.x     = spot.x;
        }
    }

    if (g->rescue_active) {
        /* Flies down to the fighter's row and docks. If there is no ship there
           yet it simply keeps station until one comes back. */
        float tx = g->player.x, ty = (float)PLAYER_Y;
        float dx = tx - g->rescue_pos.x, dy = ty - g->rescue_pos.y;
        float d  = sqrtf(dx * dx + dy * dy);
        if (d <= RESCUE_SPEED) {
            if (g->player.alive) {
                g->player.dual   = true;
                g->rescue_active = false;
                if (g->trace) printf("tick %d: dual fighter docked\n", g->tick);
            }
        } else {
            g->rescue_pos.x += dx / d * RESCUE_SPEED;
            g->rescue_pos.y += dy / d * RESCUE_SPEED;
        }
    }

    collide_shots(g);
    collide_player(g);

    if (wave_cleared(&g->wave) && g->stage_clear == 0) {
        g->stage_clear = STAGE_PAUSE;

        /* A bonus round pays out when it ends: a flat rate per flyer caught on
           the way through, and a much larger bonus for catching every one. */
        if (wave_is_challenge(&g->wave)) {
            g->bonus_hits  = wave_challenge_hits(&g->wave);
            g->bonus_award = (g->bonus_hits >= MAX_ENEMIES) ? CHALLENGE_PERFECT : 0;
            g->score += g->bonus_award;
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

void game_draw(Gfx *gfx, const Game *g)
{
    stars_draw(gfx);
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

    /* HUD. The score uses the debug font because the sheet carries no
       alphabet - only the boss score values, which are sprites. */
    char buf[32];
    font_draw(gfx, 4, 2, WHITE, "1UP");
    snprintf(buf, sizeof buf, "%d", g->score);
    font_draw(gfx, 4, 10, YELLOW, buf);

    if (wave_is_challenge(&g->wave)) {
        snprintf(buf, sizeof buf, "BONUS %d", g->stage);
    } else {
        snprintf(buf, sizeof buf, "STAGE %d", g->stage);
    }
    font_draw(gfx, GAME_W - font_width(buf) - 4, 2, CYAN, buf);

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
