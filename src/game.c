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

#define PLAYER_SPEED    1.6f
#define SHOT_SPEED      4.0f
#define FIRE_COOLDOWN   12
#define RESPAWN_TICKS   70
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

/* ------------------------------------------------------------------ setup */

static void clear_shots(Game *g);

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

    g->player.x       = GAME_W / 2.0f;
    g->player.alive   = true;
    g->player.lives   = START_LIVES;
    g->player.respawn = 0;

    clear_shots(g);
    g->fire_cooldown = 0;

    fx_reset(&g->fx);
    wave_restart(&g->wave);
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

static void kill_player(Game *g)
{
    if (!g->player.alive || g->invulnerable) return;
    g->player.alive   = false;
    g->player.respawn = RESPAWN_TICKS;
    fx_blast_player(&g->fx, player_pos(g));
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
            bool killed = wave_hit(&g->wave, e, &score, &popup);

            sh->alive = false;
            if (killed) {
                g->score += score;
                fx_blast_enemy(&g->fx, at);
                if (popup > 0) fx_score(&g->fx, at, popup);
            }
            break;
        }
    }
}

/* The wave against the fighter: missiles, and the divers themselves. */
static void collide_player(Game *g)
{
    if (!g->player.alive) return;
    Vec2 p = player_pos(g);

    for (int i = 0; i < MAX_ENEMY_SHOTS; ++i) {
        EnemyShot *s = &g->wave.shot[i];
        if (!s->alive) continue;
        if (dist2(s->pos, p) <= R_MISSILE_SHIP * R_MISSILE_SHIP) {
            s->alive = false;
            kill_player(g);
            return;
        }
    }

    /* Only a diver can reach the fighter, but testing every live enemy costs
       nothing and means a stray one cannot pass through it unnoticed. */
    for (int e = 0; e < MAX_ENEMIES; ++e) {
        const Enemy *en = &g->wave.enemies[e];
        if (en->state == ENEMY_DEAD || en->state == ENEMY_WAITING) continue;
        if (dist2(en->pos, p) <= R_ENEMY_SHIP * R_ENEMY_SHIP) {
            kill_player(g);
            return;
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

static void fire(Game *g)
{
    for (int i = 0; i < MAX_SHOTS; ++i) {
        if (g->shots[i].alive) continue;
        g->shots[i].alive = true;
        g->shots[i].pos.x = g->player.x;
        g->shots[i].pos.y = (float)(PLAYER_Y - 8);
        g->fire_cooldown  = FIRE_COOLDOWN;
        return;
    }
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
        if (--g->stage_clear == 0) {
            ++g->stage;
            clear_shots(g);        /* nothing from the old wave survives */
            wave_restart(&g->wave);
            if (g->trace) trace_new_wave(g);
        }
        return;
    }

    wave_update(&g->wave, g->player.x);

    if (g->player.alive) {
        if (keys[SDL_SCANCODE_LEFT])  g->player.x -= PLAYER_SPEED;
        if (keys[SDL_SCANCODE_RIGHT]) g->player.x += PLAYER_SPEED;

        const float half = CELL / 2.0f;
        if (g->player.x < half)              g->player.x = half;
        if (g->player.x > GAME_W - half)     g->player.x = GAME_W - half;

        if (g->fire_cooldown > 0) --g->fire_cooldown;
        if (keys[SDL_SCANCODE_SPACE] && g->fire_cooldown == 0) fire(g);
    } else {
        /* Respawn once the countdown has run out *and* the explosion has
           finished playing, so a fresh ship never appears inside the wreck of
           the old one. */
        if (g->player.respawn > 0) --g->player.respawn;
        if (g->player.respawn == 0 && !fx_player_blast_active(&g->fx)) {
            g->player.alive = true;
            g->player.x     = GAME_W / 2.0f;
        }
    }

    collide_shots(g);
    collide_player(g);

    if (wave_cleared(&g->wave) && g->stage_clear == 0) {
        g->stage_clear = STAGE_PAUSE;

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
        Vec2 p = { g->player.x, (float)PLAYER_Y };
        shape_draw(gfx, SHP_FIGHTER, p, HEADING_N, 1.0f);
    }

    fx_draw(gfx, &g->fx);

    /* HUD. The score uses the debug font because the sheet carries no
       alphabet - only the boss score values, which are sprites. */
    char buf[32];
    font_draw(gfx, 4, 2, WHITE, "1UP");
    snprintf(buf, sizeof buf, "%d", g->score);
    font_draw(gfx, 4, 10, YELLOW, buf);

    snprintf(buf, sizeof buf, "STAGE %d", g->stage);
    font_draw(gfx, GAME_W - font_width(buf) - 4, 2, CYAN, buf);

    /* Spare lives along the bottom left, drawn as small copies of the ship
       itself rather than a separate icon - one more thing the vector artwork
       gets for nothing. The stage flags that used to sit bottom right were
       sheet art and a fixed decoration wired to nothing, so they are gone
       until they can be drawn as shapes and driven by the stage count. */
    for (int i = 0; i < g->player.lives - 1 && i < 5; ++i) {
        Vec2 p = { 9.0f + i * 13.0f, (float)(GAME_H - 9) };
        shape_draw(gfx, SHP_FIGHTER, p, HEADING_N, 0.72f);
    }

    if (g->game_over > 0) {
        const char *msg = "GAME OVER";
        font_draw(gfx, (GAME_W - font_width(msg)) / 2, GAME_H / 2 - 4, RED, msg);
    } else if (g->stage_clear > 0) {
        const char *msg = "STAGE CLEAR";
        font_draw(gfx, (GAME_W - font_width(msg)) / 2, GAME_H / 2 - 4, CYAN, msg);
    }
}
