/* Claudaga.
 *
 * Opens on the title screen; START runs the game. Behind play on Tab sit two
 * tools: the shape browser, which shows the vector artwork and the font at a
 * size where they can be judged, and the pose check, which drives a shape
 * through a full circle of headings.
 *
 * Nothing here loads an image. The project began by indexing a ripped arcade
 * sprite sheet; every one of those thirty groups is now generated - polygons
 * for the artwork, strokes for the text, per-frame geometry for the explosions
 * and the tractor beam - so the sheet, its atlas and the browser that displayed
 * it have all gone. */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "gfx.h"
#include "font.h"
#include "shape.h"
#include "game.h"
#include "audio.h"
#include "input.h"

/* A headless fast-forward starts a fresh game rather than stopping on the
   menu. Flip this to exercise the interactive path through --at. */
#define WARMUP_TO_TITLE false

static const SDL_Color YELLOW = { 255, 216,   0, 255 };
static const SDL_Color CYAN   = {   0, 224, 255, 255 };
static const SDL_Color DIM    = { 144, 144, 160, 255 };
static const SDL_Color PALE   = { 230, 230, 240, 255 };

/* The title is where the game opens; the tools sit behind play on Tab. */
typedef enum { VIEW_TITLE, VIEW_PLAY, VIEW_SHAPES, VIEW_POSE, VIEW_COUNT } View;

/* Options has nothing in it yet, so it is reachable from the title rather than
   sitting in the Tab rotation. */
typedef enum { MENU_START, MENU_OPTIONS, MENU_QUIT, MENU_COUNT } MenuItem;

/* --------------------------------------------------------------- title */

/* The wordmark. Drawn a letter at a time so it can run through a colour ramp,
   with a dark pass offset behind it for depth - which is most of what makes an
   arcade logo look like one rather than like a line of text. */
static void draw_logo(Gfx *g, float cx, float top, float scale)
{
    static const char *TITLE = "CLAUDAGA";
    /* A warm arch, brightest in the middle. A ramp that ran all the way to
       cool put a pale blue letter on the end of the word, which broke it. */
    static const SDL_Color RAMP[8] = {
        { 255,  72,  56, 255 }, { 255, 118,  40, 255 },
        { 255, 166,  40, 255 }, { 255, 212,  56, 255 },
        { 255, 240,  96, 255 }, { 255, 206,  48, 255 },
        { 255, 158,  40, 255 }, { 255, 104,  44, 255 },
    };
    static const SDL_Color SHADOW = { 24, 32, 96, 255 };

    float w   = font_width_scaled(TITLE, scale);
    float x0  = cx - w * 0.5f;
    float off = scale * 0.55f;

    for (int pass = 0; pass < 2; ++pass) {
        float x = x0 + (pass ? 0.0f : off);
        float y = top + (pass ? 0.0f : off);
        for (int i = 0; i < 8; ++i) {
            char one[2] = { TITLE[i], 0 };
            font_draw_scaled(g, x, y, pass ? RAMP[i] : SHADOW, one, scale);
            x += FONT_ADVANCE * scale;
        }
    }
}

static void title_draw(Gfx *g, int menu_sel, int tick)
{
    game_background_draw(g);

    draw_logo(g, GAME_W * 0.5f, 40.0f, 3.4f);

    const char *sub = "A GALAGA CLONE IN C99 AND SDL2";
    font_draw(g, (GAME_W - font_width(sub)) / 2, 86, DIM, sub);

    /* A few of the cast, so the title screen shows what the game is. */
    static const ShapeId CAST[4] = { SHP_BEE, SHP_BUTTERFLY, SHP_BOSS, SHP_MOTH };
    for (int i = 0; i < 4; ++i) {
        Vec2 p = { 48.0f + i * 43.0f, 116.0f };
        shape_draw(g, CAST[i], p, HEADING_N, 1.5f);
    }

    static const char *ITEMS[MENU_COUNT] = { "START", "OPTIONS", "QUIT" };
    for (int i = 0; i < MENU_COUNT; ++i) {
        float y = 164.0f + i * 22.0f;
        bool  on = (i == menu_sel);
        int   x  = (GAME_W - font_width(ITEMS[i])) / 2;
        font_draw(g, x, (int)y, on ? YELLOW : DIM, ITEMS[i]);

        /* The fighter itself is the cursor. */
        if (on) {
            Vec2 c = { (float)x - 14.0f, y + FONT_H * 0.5f };
            shape_draw(g, SHP_FIGHTER, c, HEADING_E, 1.0f);
        }
    }

    if ((tick / 30) % 2 == 0) {
        const char *hint = "ARROWS SELECT   ENTER CONFIRM";
        font_draw(g, (GAME_W - font_width(hint)) / 2, GAME_H - 24, CYAN, hint);
    }
    font_draw(g, 4, GAME_H - 9, DIM, "TAB TOOLS");
}

static void options_draw(Gfx *g)
{
    game_background_draw(g);
    const char *h = "OPTIONS";
    font_draw_scaled(g, (GAME_W - font_width_scaled(h, 2.0f)) / 2, 50.0f,
                     YELLOW, h, 2.0f);

    /* Nothing is configurable yet, but what is plugged in is worth saying:
       "does it see my controller" is the first question anyone asks, and a
       menu that cannot answer it sends them to the game to find out. */
    char buf[48];
    int pads = input_pads();
    if (pads > 0) {
        snprintf(buf, sizeof buf, "CONTROLLER  %d CONNECTED", pads);
    } else {
        snprintf(buf, sizeof buf, "CONTROLLER  NONE");
    }
    font_draw(g, (GAME_W - font_width(buf)) / 2, 108, pads > 0 ? CYAN : DIM, buf);

    /* The controls as three columns rather than two centred sentences. The
       sentences were 38 characters, and 38 characters at a six pixel advance
       is 228 - four pixels wider than the screen, so both ends were clipped.
       Columns keep every string short enough that no arrangement of them can
       run off the edge, and they line up, which a pair of separately centred
       lines never does. */
    const int COL_WHO = 30, COL_FLY = 72, COL_FIRE = 166;

    font_draw(g, COL_FLY,  126, DIM, "FLY");
    font_draw(g, COL_FIRE, 126, DIM, "FIRE");

    font_draw(g, COL_WHO,  140, CYAN, "PAD");
    font_draw(g, COL_FLY,  140, PALE, "STICK OR DPAD");
    font_draw(g, COL_FIRE, 140, PALE, "A");

    font_draw(g, COL_WHO,  152, CYAN, "KEYS");
    font_draw(g, COL_FLY,  152, PALE, "ARROWS");
    font_draw(g, COL_FIRE, 152, PALE, "SPACE");

    const char *a = "VOLUME SETTINGS TO COME";
    font_draw(g, (GAME_W - font_width(a)) / 2, 176, DIM, a);

    const char *back = "ESC BACK";
    font_draw(g, (GAME_W - font_width(back)) / 2, GAME_H - 40, CYAN, back);
}

/* ----------------------------------------------------------- shape browser */

/* Every vector shape at a readable size, plus the two recoloured variants.
   Judging artwork needs it big and still; rotation is checked by the pose
   view, which drives the same shapes through a full circle. */
static void shapes_draw(Gfx *g, int tick)
{
    (void)tick;
    font_draw(g, 4, 2, YELLOW, "VECTOR SHAPES");

    /* Four columns: ten shapes plus the two recolours, which are the same
       drawings in different palettes and belong beside their originals. */
    static const struct { ShapeId id; const ShapePalette *pal; const char *name; }
    CELLS[] = {
        { SHP_FIGHTER,     NULL, "FIGHTER"  }, { SHP_BEE,      NULL, "BEE"      },
        { SHP_BUTTERFLY,   NULL, "BFLY"     }, { SHP_BOSS,     NULL, "BOSS"     },
        { SHP_PLAYER_SHOT, NULL, "SHOT"     }, { SHP_ENEMY_SHOT, NULL, "MISSILE" },
        { SHP_FIGHTER,     &SHAPE_PAL_FIGHTER_CAPTURED, "TAKEN" },
        { SHP_BOSS,        &SHAPE_PAL_BOSS_HIT,         "HIT"   },
        { SHP_MOTH,        NULL, "MOTH"     }, { SHP_SCORPION, NULL, "SCORP"    },
        { SHP_DART,        NULL, "DART"     }, { SHP_ORB,      NULL, "ORB"      },
    };

    const float S = 2.0f;
    for (int i = 0; i < (int)ARRAY_COUNT(CELLS); ++i) {
        Vec2 p = { 30.0f + (i % 4) * 48.0f, 40.0f + (i / 4) * 56.0f };
        shape_draw_pal(g, CELLS[i].id, p, 0.0f, S, CELLS[i].pal, 1.0f);
        font_draw(g, (int)p.x - font_width(CELLS[i].name) / 2, (int)p.y + 22,
                  DIM, CELLS[i].name);
    }

    /* The font is vector artwork too, and the only way to judge glyphs is to
       see them together at a size where the strokes are separable. */
    font_draw(g, 4, 214, YELLOW, "FONT");
    font_draw_scaled(g, 4.0f, 228.0f, DIM, "ABCDEFGHIJKLM", 1.6f);
    font_draw_scaled(g, 4.0f, 244.0f, DIM, "NOPQRSTUVWXYZ", 1.6f);
    font_draw_scaled(g, 4.0f, 260.0f, DIM, "0123456789-:/", 1.6f);

    font_draw(g, 4, GAME_H - 9, CYAN, "TAB VIEW  ESC QUIT");
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

/* One simulation tick of the play view, including what happens when the game
   reports itself finished. Shared so the interactive loop and the headless
   fast-forward cannot drift apart: `to_title` hands control back to the menu,
   which is what a person should see, while a headless run has nobody to show a
   menu to and simply starts again. */
static void play_tick(Game *game, const Input *in, bool to_title,
                      View *view, int *menu_sel)
{
    game_update(game, in);
    if (!game->finished) return;

    if (to_title) {
        game->finished = false;
        *menu_sel      = MENU_START;
        *view          = VIEW_TITLE;
    } else {
        game_restart(game);
    }
}

static void usage(void)
{
    fprintf(stderr,
            "usage: claudaga [--title] [--scene] [--shapes] [--pose]\n"
            "                [--subject N] [--scale N]\n"
            "                [--at TICK] [--paths] [--observe] [--autofire]\n"
            "                [--trace] [--shot out.bmp] [--stats N]\n"
            "                [--stage N] [--mute] [--padtest] [--options]\n"
            "\n"
            "the game starts by default; the view flags select a tool instead\n");
}

/* The menu, as actions rather than as key handlers.
 *
 * Both the keyboard and a controller drive these, and they have to stay the
 * same menu: written twice, one of them acquires a case the other lacks the
 * first time anything is added. It is the same reasoning that put the warm-up
 * and the interactive loop through one play_tick. */
typedef struct {
    View view;
    int  menu_sel;
    bool options;
    bool running;
    int  subject;
} Ui;

static void ui_up(Ui *u)
{
    if (u->view == VIEW_TITLE && !u->options) {
        u->menu_sel = (u->menu_sel + MENU_COUNT - 1) % MENU_COUNT;
    }
}

static void ui_down(Ui *u)
{
    if (u->view == VIEW_TITLE && !u->options) {
        u->menu_sel = (u->menu_sel + 1) % MENU_COUNT;
    }
}

static void ui_confirm(Ui *u, Game *game)
{
    if (u->view != VIEW_TITLE || u->options) return;

    if (u->menu_sel == MENU_START) {
        game_restart(game);
        u->view = VIEW_PLAY;
        audio_music_stop();
    } else if (u->menu_sel == MENU_OPTIONS) {
        u->options = true;
    } else {
        u->running = false;
    }
}

/* Always towards the menu, never out of the process: out of the options page
   first, then out of whatever view is up. On the title itself there is nowhere
   further back to go, and QUIT is the way out. */
static void ui_back(Ui *u)
{
    if (u->options)                 u->options = false;
    else if (u->view != VIEW_TITLE) u->view = VIEW_TITLE;
}

static void ui_next_view(Ui *u)
{
    if (!u->options) u->view = (View)((u->view + 1) % VIEW_COUNT);
}

int main(int argc, char **argv)
{
    const char *shot_path = NULL;
    Ui          ui        = { VIEW_TITLE, MENU_START, false, true, 0 };
    bool        view_set  = false;
    int         scale     = 3;
    int         warmup    = 0;
    int         stats     = 0;
    bool        paths     = false;
    bool        observe   = false;
    int         first_stage = 1;
    bool        mute      = false;
    bool        padtest   = false;
    bool        show_options = false;
    bool        autofire  = false;
    bool        trace     = false;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc)          shot_path = argv[++i];
        else if (!strcmp(argv[i], "--scene"))   { ui.view = VIEW_PLAY;    view_set = true; }
        else if (!strcmp(argv[i], "--pose"))    { ui.view = VIEW_POSE;    view_set = true; }
        else if (!strcmp(argv[i], "--shapes"))  { ui.view = VIEW_SHAPES;  view_set = true; }
        else if (!strcmp(argv[i], "--title"))   { ui.view = VIEW_TITLE;   view_set = true; }
        else if (!strcmp(argv[i], "--subject") && i + 1 < argc)  ui.subject = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scale")   && i + 1 < argc)  scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--at")      && i + 1 < argc)  warmup = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--stats")   && i + 1 < argc)  stats = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--stage")   && i + 1 < argc)  first_stage = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--paths"))                    paths = true;
        else if (!strcmp(argv[i], "--observe"))                  observe = true;
        else if (!strcmp(argv[i], "--autofire"))                 autofire = true;
        else if (!strcmp(argv[i], "--trace"))                    trace = true;
        else if (!strcmp(argv[i], "--mute"))                     mute = true;
        else if (!strcmp(argv[i], "--padtest"))                  padtest = true;
        else if (!strcmp(argv[i], "--options")) { show_options = true; view_set = true; }
        else { usage(); return 1; }
    }
    if (scale < 1) scale = 1;

    /* Fast-forwarding or measuring means the game, not the title: there is
       nothing to advance on a menu, and every capture would otherwise come
       back as a picture of the title screen. */
    /* The options page is an overlay rather than a view, so it needs its own
       flag to be screenshot. It had none, which is exactly why a line four
       pixels too wide for the screen shipped: nothing ever drew it except a
       person clicking through the menu. */
    if (show_options) { ui.view = VIEW_TITLE; ui.options = true; }

    if (!view_set && (warmup > 0 || stats > 0)) ui.view = VIEW_PLAY;
    if (ui.subject < 0 || ui.subject >= ARRAY_COUNT(POSE_SUBJECTS)) ui.subject = 0;

    /* Before the window: the self test wants SDL up but has nothing to draw,
       and opening a window it would immediately close is noise. */
    if (padtest) {
        if (SDL_Init(SDL_INIT_EVENTS) < 0) {
            fprintf(stderr, "SDL would not start: %s' + BS + 'n", SDL_GetError());
            return 2;
        }
        int bad = input_selftest();
        SDL_Quit();
        return bad == 0 ? 0 : 1;
    }

    Gfx g;
    if (!gfx_init(&g, "Claudaga", scale)) return 1;

    /* A headless run mutes itself. Opening a device to render one screenshot
       is pointless, and a --stats run would fire thousands of effects at a
       device nobody is listening to - which is slow, and on some drivers is
       slow enough to matter to a measurement. */
    audio_init(!mute && !shot_path && stats <= 0);

    /* Controllers are opened even for a headless run. The warm-up drives the
       game from a struct it fills in itself and never reads a pad, but a run
       that fast-forwards and then hands over wants one working when it does. */
    input_open();

    static Game game;   /* several hundred KB of baked paths; not stack-sized */
    game_init(&game);

    /* Starting later is a measurement tool rather than a cheat: the difficulty
       ramp only shows itself over a dozen stages, and playing up to stage 12 to
       check a number is not a test anybody runs twice. */
    if (first_stage > 1) {
        game.first_stage = first_stage;
        game_restart(&game);
    }

    game.wave.show_paths = paths;
    game.invulnerable    = observe;
    game.trace           = trace;

    /* --stats exercises the wave on its own rather than the whole game: who
       attacks and how often is a property of the wave, and letting the fighter
       die mid-run would restart it and skew the tally. */
    if (stats > 0) {
        for (int i = 0; i < stats; ++i) wave_update(&game.wave, game.player.x);
        wave_print_stats(&game.wave);
        input_close();
        audio_shutdown();
        gfx_shutdown(&g);
        return 0;
    }

    /* --at runs the simulation forward with no rendering, so a screenshot can
       be taken at a chosen moment rather than only at tick 0. Pair it with
       --observe to stop the fighter dying and restarting the run. */
    Input warm = { false, false, false };
    warm.fire = autofire;
    for (int i = 0; i < warmup; ++i) {
        /* Autofire also sweeps the fighter side to side. Parked in the middle
           it only ever shoots up one column, so a wave never clears and the
           later stages cannot be reached to look at. */
        if (autofire) {
            int leftward = (i / 90) % 2;
            warm.left  = leftward != 0;
            warm.right = leftward == 0;
        }
        play_tick(&game, &warm, WARMUP_TO_TITLE, &ui.view, &ui.menu_sel);
    }

    /* Fixed 60Hz steps with an accumulator, so the simulation does not change
       speed if the display refreshes at some other rate. */
    const double STEP = 1.0 / 60.0;
    double freq  = (double)SDL_GetPerformanceFrequency();
    Uint64 prev  = SDL_GetPerformanceCounter();
    double accum = 0.0;

    int  tick      = 0;
    View shown     = ui.view;

    while (ui.running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            input_event(&ev);

            if (ev.type == SDL_QUIT) {
                ui.running = false;
            } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE:      ui_back(&ui);            break;
                case SDLK_TAB:         ui_next_view(&ui);       break;
                case SDLK_UP:          ui_up(&ui);              break;
                case SDLK_DOWN:        ui_down(&ui);            break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:    ui_confirm(&ui, &game);  break;
                case SDLK_r:
                    if (ui.view == VIEW_PLAY) game_restart(&game);
                    break;
                case SDLK_p:
                    if (ui.view == VIEW_PLAY) {
                        game.wave.show_paths = !game.wave.show_paths;
                    }
                    break;
                case SDLK_a:
                    if (ui.view == VIEW_PLAY) {
                        game.wave.attacks_enabled = !game.wave.attacks_enabled;
                    }
                    break;
                case SDLK_LEFT:
                case SDLK_PAGEUP:
                    if (ui.view == VIEW_POSE) {
                        ui.subject = (ui.subject + ARRAY_COUNT(POSE_SUBJECTS) - 1)
                                % ARRAY_COUNT(POSE_SUBJECTS);
                    }
                    break;
                case SDLK_RIGHT:
                case SDLK_PAGEDOWN:
                    if (ui.view == VIEW_POSE) {
                        ui.subject = (ui.subject + 1) % ARRAY_COUNT(POSE_SUBJECTS);
                    }
                    break;
                default: break;
                }
            }
        }

        /* A controller's menu presses arrive here rather than in the switch
           above, because some of them are stick crossings rather than button
           events and are only noticed when the sticks are sampled. They go
           through the very same handlers the keys do. */
        for (UiAction a = input_take_ui(); a != UI_NONE; a = input_take_ui()) {
            switch (a) {
            case UI_UP:        ui_up(&ui);             break;
            case UI_DOWN:      ui_down(&ui);           break;
            case UI_CONFIRM:   ui_confirm(&ui, &game); break;
            case UI_BACK:      ui_back(&ui);           break;
            case UI_NEXT_VIEW: ui_next_view(&ui);      break;
            case UI_NONE:                              break;
            }
        }

        /* Leaving any view for the game silences whatever was playing. Doing
           it on the transition rather than at the menu covers Tab as well,
           which can drop straight into play from a shape tool. */
        if (ui.view != shown) {
            if (ui.view == VIEW_PLAY) audio_music_stop();
            shown = ui.view;
        }

        Uint64 now = SDL_GetPerformanceCounter();
        accum += (double)(now - prev) / freq;
        prev = now;
        if (accum > 0.25) accum = 0.25;   /* do not spiral after a stall */

        /* Sampled once a frame, not once a step: two simulation steps inside
           one frame see the same input, which is what the accumulator is for.
           This also feeds the stick crossings into the queue drained above. */
        Input in;
        input_sample(&in);

        while (accum >= STEP) {
            accum -= STEP;
            ++tick;
            if (ui.view == VIEW_PLAY) {
                play_tick(&game, &in, true, &ui.view, &ui.menu_sel);
            } else {
                game_background_update();

                /* Everything that is not the game runs under the title music -
                   the menu, the options page and both shape tools. Asking for
                   a track already playing does nothing, so this needs no state
                   of its own. */
                audio_music(MUSIC_TITLE);
            }
        }

        gfx_begin_frame(&g);
        if (ui.options)                  options_draw(&g);
        else if (ui.view == VIEW_TITLE)  title_draw(&g, ui.menu_sel, tick);
        else if (ui.view == VIEW_PLAY)   game_draw(&g, &game);
        else if (ui.view == VIEW_POSE)   pose_draw(&g, ui.subject);
        else                          shapes_draw(&g, tick);

        if (shot_path) {
            gfx_screenshot(&g, shot_path);
            printf("wrote %s\n", shot_path);
            ui.running = false;
        }
        gfx_end_frame(&g);
    }

    input_close();
    audio_shutdown();
    gfx_shutdown(&g);
    return 0;
}
