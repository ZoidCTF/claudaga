/* Claudaga.
 *
 * The play view is the game and is what starts. Behind it on Tab sit three
 * tools: the shape browser shows the vector artwork and the font at a size
 * where they can be judged, the pose check drives a shape through a full
 * circle of headings, and the sprite browser shows the original arcade sheet
 * the designs were drawn from. That last one is reference material only - the
 * game does not read the sheet and does not need it present. */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "gfx.h"
#include "atlas.h"
#include "font.h"
#include "shape.h"
#include "game.h"

#define SHEET_PATH "assets/galaga_sheet.png"

static const SDL_Color YELLOW = { 255, 216,   0, 255 };
static const SDL_Color CYAN   = {   0, 224, 255, 255 };
static const SDL_Color DIM    = { 144, 144, 160, 255 };

/* Play first: the tools sit behind it on Tab rather than in front of it. */
typedef enum { VIEW_PLAY, VIEW_SHAPES, VIEW_POSE, VIEW_BROWSER, VIEW_COUNT } View;

/* ----------------------------------------------------------- shape browser */

/* Every vector shape at a readable size, plus the two recoloured variants.
   Judging artwork needs it big and still; rotation is checked by the pose
   view, which drives the same shapes through a full circle. */
static void shapes_draw(Gfx *g, int tick)
{
    (void)tick;
    font_draw(g, 4, 2, YELLOW, "VECTOR SHAPES");

    static const ShapeId row[6] = {
        SHP_FIGHTER, SHP_BEE, SHP_BUTTERFLY,
        SHP_BOSS, SHP_PLAYER_SHOT, SHP_ENEMY_SHOT,
    };
    const float S = 2.6f;

    for (int i = 0; i < 6; ++i) {
        Vec2 p = { 40.0f + (i % 3) * 72.0f, 48.0f + (i / 3) * 84.0f };
        shape_draw(g, row[i], p, 0.0f, S);
        const char *n = shape_name(row[i]);
        font_draw(g, (int)p.x - font_width(n) / 2, (int)p.y + 30, DIM, n);
    }

    /* The alternative palettes, which on the sheet needed whole second sets of
       frames drawn out. */
    font_draw(g, 4, 150, YELLOW, "RECOLOURS");
    Vec2 a = { 56.0f, 186.0f };
    Vec2 b = { 152.0f, 186.0f };
    shape_draw_pal(g, SHP_FIGHTER, a, 0.0f, S, &SHAPE_PAL_FIGHTER_CAPTURED, 1.0f);
    shape_draw_pal(g, SHP_BOSS,    b, 0.0f, S, &SHAPE_PAL_BOSS_HIT,         1.0f);
    font_draw(g, (int)a.x - font_width("CAPTURED") / 2, 212, DIM, "CAPTURED");
    font_draw(g, (int)b.x - font_width("BOSS HIT") / 2, 212, DIM, "BOSS HIT");

    /* The font is vector artwork too, and the only way to judge glyphs is to
       see them together at a size where the strokes are separable. */
    font_draw(g, 4, 228, YELLOW, "FONT");
    font_draw_scaled(g, 4.0f, 240.0f, DIM, "ABCDEFGHIJKLM", 1.6f);
    font_draw_scaled(g, 4.0f, 256.0f, DIM, "NOPQRSTUVWXYZ", 1.6f);
    font_draw_scaled(g, 4.0f, 272.0f, DIM, "0123456789-:/", 1.6f);
}

/* ---------------------------------------------------------------- browser */

#define BROWSER_TOP    12
#define BROWSER_BOTTOM (GAME_H - 12)
#define LABEL_GAP      2
#define ENTRY_GAP      4

/* Height one sprite group needs: its label, then its tallest frame. */
static int entry_height(SpriteId id)
{
    const Sprite *s = atlas_get(id);
    int art = s->count > 0 ? s->frame[0].h : 0;
    return FONT_H + LABEL_GAP + art;
}

/* Walks the sprite list splitting it into screenfuls. Returns the page count
   and fills `starts` with the first sprite on each page. */
static int paginate(int *starts, int max_pages)
{
    int pages = 0, y = BROWSER_TOP;
    starts[0] = 0;
    for (int id = 0; id < SPR_COUNT; ++id) {
        int h = entry_height((SpriteId)id);
        if (y + h > BROWSER_BOTTOM && id > starts[pages]) {
            if (++pages >= max_pages) return max_pages;
            starts[pages] = id;
            y = BROWSER_TOP;
        }
        y += h + ENTRY_GAP;
    }
    return pages + 1;
}

static void browser_draw(Gfx *g, int page, int page_count, const int *starts,
                         int tick, bool have_sheet)
{
    if (!have_sheet) {
        font_draw(g, 4, 2, YELLOW, "SPRITE ATLAS");
        font_draw(g, 4, 16, DIM, "NO SHEET LOADED - NOTHING TO SHOW");
        font_draw(g, 4, 26, DIM, "THE GAME DOES NOT NEED IT");
        font_draw(g, 4, GAME_H - 9, CYAN, "TAB VIEW  ESC QUIT");
        return;
    }

    char head[64];
    snprintf(head, sizeof head, "SPRITE ATLAS  PAGE %d/%d", page + 1, page_count);
    font_draw(g, 4, 2, YELLOW, head);

    int first = starts[page];
    int last  = (page + 1 < page_count) ? starts[page + 1] : SPR_COUNT;

    int y = BROWSER_TOP;
    for (int id = first; id < last; ++id) {
        const Sprite *s = atlas_get((SpriteId)id);

        char label[64];
        snprintf(label, sizeof label, "%s %d", s->name, s->count);
        font_draw(g, 4, y, DIM, label);

        /* Frames run left to right at their natural size. The widest group on
           the sheet is the 5-frame enemy blast at 170px, so nothing clips. */
        int x = 4;
        for (int f = 0; f < s->count; ++f) {
            gfx_blit(g, &s->frame[f], x, y + FONT_H + LABEL_GAP);
            x += s->frame[f].w + 2;
        }

        /* A lone copy on the right cycles through the frames, which makes the
           animations and the rotation order easy to read at a glance. */
        if (s->count > 1) {
            const SDL_Rect *cur = atlas_frame((SpriteId)id, tick / 8);
            if (x + cur->w + 6 <= GAME_W) {
                gfx_blit(g, cur, GAME_W - cur->w - 4, y + FONT_H + LABEL_GAP);
            }
        }

        y += entry_height((SpriteId)id) + ENTRY_GAP;
    }

    font_draw(g, 4, GAME_H - 9, CYAN, "TAB VIEW  < > PAGE  ESC QUIT");
}

/* ------------------------------------------------------------- pose check */

/* Draws one flyer at headings all the way round a circle, each copy placed in
   the direction it is supposed to be facing, so every one should point
   straight out from the centre like a spoke.

   This mattered a great deal with the sprite sheet, where a heading had to be
   resolved to one of seven stored frames plus a choice of mirrorings and any
   mistake in that mapping put a ship on backwards. The shapes simply rotate,
   so the check is close to a formality now - but it is the thing that would
   catch a sign error in the rotation matrix, and it costs nothing to keep. */

#define POSE_STEPS 24

static const ShapeId POSE_SUBJECTS[] = {
    SHP_FIGHTER, SHP_BEE, SHP_BUTTERFLY, SHP_BOSS,
};

static void pose_draw(Gfx *g, int subject)
{
    ShapeId id = POSE_SUBJECTS[subject];

    char head[64];
    snprintf(head, sizeof head, "POSE CHECK  %s", shape_name(id));
    font_draw(g, 4, 2, YELLOW, head);
    font_draw(g, 4, 11, DIM, "ALL SHOULD POINT OUTWARD");

    const float cx = GAME_W / 2.0f;
    const float cy = 158.0f;
    const float r  = 84.0f;

    for (int i = 0; i < POSE_STEPS; ++i) {
        float heading = 360.0f * (float)i / (float)POSE_STEPS;
        float rad     = heading * (float)M_PI / 180.0f;

        /* Place it in the direction it claims to face: north is -y. */
        Vec2 p = { cx + sinf(rad) * r, cy - cosf(rad) * r };
        shape_draw(g, id, p, heading, 1.3f);
    }

    /* The same shape upright in the middle, for comparison. */
    Vec2 mid = { cx, cy };
    shape_draw(g, id, mid, HEADING_N, 1.3f);

    font_draw(g, 4, GAME_H - 9, CYAN, "TAB VIEW  < > SHAPE  ESC QUIT");
}

/* ------------------------------------------------------------------- main */

static void usage(void)
{
    fprintf(stderr,
            "usage: claudaga [--shapes] [--pose] [--browser] [--scene]\n"
            "                [--page N] [--subject N] [--scale N]\n"
            "                [--at TICK] [--paths] [--observe] [--autofire]\n"
            "                [--trace] [--shot out.bmp] [--stats N]\n"
            "\n"
            "the game starts by default; the view flags select a tool instead\n");
}

int main(int argc, char **argv)
{
    const char *shot_path = NULL;
    View        view      = VIEW_PLAY;
    int         page      = 0;
    int         subject   = 0;
    int         scale     = 3;
    int         warmup    = 0;
    int         stats     = 0;
    bool        paths     = false;
    bool        observe   = false;
    bool        autofire  = false;
    bool        trace     = false;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc)          shot_path = argv[++i];
        else if (!strcmp(argv[i], "--scene"))                    view = VIEW_PLAY;
        else if (!strcmp(argv[i], "--pose"))                     view = VIEW_POSE;
        else if (!strcmp(argv[i], "--shapes"))                   view = VIEW_SHAPES;
        else if (!strcmp(argv[i], "--browser"))                  view = VIEW_BROWSER;
        else if (!strcmp(argv[i], "--page")    && i + 1 < argc)  page = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--subject") && i + 1 < argc)  subject = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scale")   && i + 1 < argc)  scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--at")      && i + 1 < argc)  warmup = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--stats")   && i + 1 < argc)  stats = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--paths"))                    paths = true;
        else if (!strcmp(argv[i], "--observe"))                  observe = true;
        else if (!strcmp(argv[i], "--autofire"))                 autofire = true;
        else if (!strcmp(argv[i], "--trace"))                    trace = true;
        else { usage(); return 1; }
    }
    if (scale < 1) scale = 1;
    if (subject < 0 || subject >= ARRAY_COUNT(POSE_SUBJECTS)) subject = 0;

    Gfx g;
    if (!gfx_init(&g, "Claudaga", scale)) return 1;

    atlas_init();

    /* The sheet is no longer needed to play: everything in the game is drawn
       from vector shapes now, and it survives only so the browser can show the
       artwork this was originally built from. Missing it is therefore a note,
       not a failure - which is what lets the project ship without carrying
       somebody else's sprite sheet. */
    bool have_sheet = gfx_load_texture(&g, &g.sheet, SHEET_PATH, true);
    if (have_sheet) {
        printf("sheet %dx%d, %d sprite groups\n", g.sheet.w, g.sheet.h, (int)SPR_COUNT);
    } else {
        printf("no sheet at " SHEET_PATH " - browser view disabled, "
               "everything else is unaffected\n");
        if (view == VIEW_BROWSER) view = VIEW_SHAPES;
    }

    int starts[32];
    int page_count = paginate(starts, ARRAY_COUNT(starts));
    if (page >= page_count) page = page_count - 1;
    if (page < 0) page = 0;

    static Game game;   /* several hundred KB of baked paths; not stack-sized */
    game_init(&game);
    game.wave.show_paths = paths;
    game.invulnerable    = observe;
    game.trace           = trace;

    /* --stats exercises the wave on its own rather than the whole game: who
       attacks and how often is a property of the wave, and letting the fighter
       die mid-run would restart it and skew the tally. */
    if (stats > 0) {
        for (int i = 0; i < stats; ++i) wave_update(&game.wave, game.player.x);
        wave_print_stats(&game.wave);
        gfx_shutdown(&g);
        return 0;
    }

    /* --at runs the simulation forward with no rendering, so a screenshot can
       be taken at a chosen moment rather than only at tick 0. Pair it with
       --observe to stop the fighter dying and restarting the run. */
    static Uint8 warm_keys[SDL_NUM_SCANCODES] = { 0 };
    if (autofire) warm_keys[SDL_SCANCODE_SPACE] = 1;
    for (int i = 0; i < warmup; ++i) {
        /* Autofire also sweeps the fighter side to side. Parked in the middle
           it only ever shoots up one column, so a wave never clears and the
           later stages cannot be reached to look at. */
        if (autofire) {
            int leftward = (i / 90) % 2;
            warm_keys[SDL_SCANCODE_LEFT]  = (Uint8)(leftward ? 1 : 0);
            warm_keys[SDL_SCANCODE_RIGHT] = (Uint8)(leftward ? 0 : 1);
        }
        game_update(&game, warm_keys);
    }

    /* Fixed 60Hz steps with an accumulator, so the simulation does not change
       speed if the display refreshes at some other rate. */
    const double STEP = 1.0 / 60.0;
    double freq  = (double)SDL_GetPerformanceFrequency();
    Uint64 prev  = SDL_GetPerformanceCounter();
    double accum = 0.0;

    int  tick    = 0;
    bool running = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_TAB:    view = (View)((view + 1) % VIEW_COUNT); break;
                case SDLK_r:
                    if (view == VIEW_PLAY) game_restart(&game);
                    break;
                case SDLK_p:
                    if (view == VIEW_PLAY) {
                        game.wave.show_paths = !game.wave.show_paths;
                    }
                    break;
                case SDLK_a:
                    if (view == VIEW_PLAY) {
                        game.wave.attacks_enabled = !game.wave.attacks_enabled;
                    }
                    break;
                case SDLK_LEFT:
                case SDLK_PAGEUP:
                    if (view == VIEW_BROWSER && page > 0) --page;
                    if (view == VIEW_POSE) {
                        subject = (subject + ARRAY_COUNT(POSE_SUBJECTS) - 1)
                                % ARRAY_COUNT(POSE_SUBJECTS);
                    }
                    break;
                case SDLK_RIGHT:
                case SDLK_PAGEDOWN:
                    if (view == VIEW_BROWSER && page < page_count - 1) ++page;
                    if (view == VIEW_POSE) {
                        subject = (subject + 1) % ARRAY_COUNT(POSE_SUBJECTS);
                    }
                    break;
                default: break;
                }
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        accum += (double)(now - prev) / freq;
        prev = now;
        if (accum > 0.25) accum = 0.25;   /* do not spiral after a stall */

        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        while (accum >= STEP) {
            accum -= STEP;
            ++tick;
            if (view == VIEW_PLAY) game_update(&game, keys);
        }

        gfx_begin_frame(&g);
        if (view == VIEW_PLAY)        game_draw(&g, &game);
        else if (view == VIEW_POSE)   pose_draw(&g, subject);
        else if (view == VIEW_SHAPES) shapes_draw(&g, tick);
        else                          browser_draw(&g, page, page_count, starts,
                                                   tick, have_sheet);

        if (shot_path) {
            gfx_screenshot(&g, shot_path);
            printf("wrote %s\n", shot_path);
            running = false;
        }
        gfx_end_frame(&g);
    }

    gfx_shutdown(&g);
    return 0;
}
