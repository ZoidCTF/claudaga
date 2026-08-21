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
#include "settings.h"

/* A headless fast-forward starts a fresh game rather than stopping on the
   menu. Flip this to exercise the interactive path through --at. */
#define WARMUP_TO_TITLE false

static const SDL_Color YELLOW = { 255, 216,   0, 255 };
static const SDL_Color CYAN   = {   0, 224, 255, 255 };
static const SDL_Color DIM    = { 144, 144, 160, 255 };
static const SDL_Color PALE   = { 230, 230, 240, 255 };

/* The title is where the game opens; the tools sit behind play on Tab. */
/* The shape browser, the pose check and the debug keys are for building the
   game rather than playing it, so a release build does not carry the ways in.
   The views themselves still compile - the command-line flags that render them
   are how the artwork gets checked, and those cost nothing to leave - but Tab,
   F2, F3 and R do nothing, and the title screen stops advertising them.

   /DNDEBUG is what build.bat passes for a release; a debug build gets the
   lot. */
#ifdef NDEBUG
#define TOOLS 0
#else
#define TOOLS 1
#endif

/* VIEW_DEMO sits outside the Tab rotation: it is somewhere the game puts
   itself, not somewhere a person navigates to. */
typedef enum { VIEW_TITLE, VIEW_PLAY, VIEW_SHAPES, VIEW_POSE, VIEW_COUNT,
               VIEW_DEMO } View;

/* Options has nothing in it yet, so it is reachable from the title rather than
   sitting in the Tab rotation. */
typedef enum { MENU_ONE, MENU_TWO, MENU_OPTIONS, MENU_QUIT, MENU_COUNT } MenuItem;

/* --------------------------------------------------------------- seats */

/* Two-player alternating play, the way the cabinet did it: one fighter at a
   time, and a turn ends when you lose one.
 *
 * Each seat is a whole Game. That is not extravagance - taking turns means
 * player two's formation is their own formation, at their own stage, with
 * their own crew, waiting exactly as they left it. Sharing one Game and
 * swapping scores would be a different game entirely.
 *
 * A seat that is out of fighters, or has finished, is skipped; when both are
 * done the session is over. */
#define SEATS 2
#define HANDOVER_TICKS 110

/* The longest the game will wait for a board to go quiet before packing it
   away anyway. Ten seconds is far past anything a real settle takes - the
   measured range is 42 to 229 ticks, plus the 240 of a GAME OVER message when
   there is one - so reaching it means something is stuck, and going on is
   better than stopping. */
#define SETTLE_LIMIT 900

/* Changing hands is a sequence, not an instant.
 *
 * A fighter is lost, and the first version swapped seats on that same tick.
 * Three things were wrong with it at once: the wreck of the outgoing ship
 * carried over on to the incoming player's screen, the outgoing board vanished
 * mid-dive, and the incoming one appeared fully formed with no explanation.
 *
 * So: let the explosion finish and the air clear, lift the outgoing formation
 * off the top of the screen, announce whose turn it is, and fly the incoming
 * formation down into place. Only then do the controls go live - a board still
 * arriving is not a board you can be asked to fight. */
typedef enum {
    TURN_PLAYING,
    TURN_SETTLING,   /* the explosion, and whatever is still flying */
    TURN_LEAVING,    /* the outgoing formation lifting away         */
    TURN_RESULTS,    /* a finished player's numbers, over empty sky */
    TURN_ARRIVING    /* the incoming one coming down                */
} TurnPhase;

typedef struct {
    Game     *game;      /* points at the two Game objects main owns */
    int       seats;     /* 1 or 2 */
    int       turn;      /* whose it is */
    int       handover;  /* ticks left on the PLAYER N banner */
    TurnPhase phase;
    int       incoming;  /* the seat being handed to, during a change */
    int       settling;  /* ticks spent waiting for the board to settle */

    /* The session's own clock, which is the only one that runs continuously -
       each seat's game tick stops while the other is playing, so a per-seat
       number cannot describe a handover that spans both. */
    int       tick;
} Session;

/* Says what just happened, on the session's clock, when tracing is on. */
static void session_log(const Session *s, const char *what)
{
    if (!s->game[0].trace) return;
    printf("tick %d: %s (seat %d, scores %d and %d)\n",
           s->tick, what, s->turn + 1, s->game[0].score, s->game[1].score);
}

static bool seat_done(const Game *g)
{
    return g->finished;
}

/* The next seat with anything left to do, or -1 when the session is over. */
static int seat_next(const Session *s)
{
    for (int i = 1; i <= s->seats; ++i) {
        int k = (s->turn + i) % s->seats;
        if (!seat_done(&s->game[k])) return k;
    }
    return seat_done(&s->game[s->turn]) ? -1 : s->turn;
}

/* Each seat needs to know the other's score for the HUD, and its own number
   for the banner. Refreshed every frame rather than pushed on change, since
   there is one place that draws and it is cheaper to be right than clever. */
static void session_sync(Session *s)
{
    for (int i = 0; i < SEATS; ++i) {
        s->game[i].seat = i;
        s->game[i].other_score =
            (s->seats > 1) ? s->game[i ^ 1].score : -1;
    }
}

static void session_begin(Session *s, int seats)
{
    s->seats    = seats;
    s->turn     = 0;
    s->handover = 0;
    s->phase    = TURN_PLAYING;
    s->incoming = 0;
    s->settling = 0;
    s->tick     = 0;
    for (int i = 0; i < SEATS; ++i) {
        s->game[i].demo = false;
        game_restart(&s->game[i]);
        /* The seat that is not playing yet is finished as far as the session
           is concerned, so a one-player game never waits on it. */
        if (i >= seats) s->game[i].finished = true;
    }
    session_sync(s);
}

/* One tick of a session: the turn that is running, the banner between turns,
   and noticing when everybody is out.
 *
 * Shared by the interactive loop and the headless warm-up for the same reason
 * play_tick is: a turn taken two different ways is two turns that will
 * eventually disagree, and the one the harness drives is the one nobody
 * watches. */
/* Passes the controls to `next`: announce it, and put its board off the top
   of the screen so it has somewhere to fly down from. */
static void session_hand_to(Session *s, int next)
{
    s->turn     = next;
    s->handover = HANDOVER_TICKS;
    audio_play(SFX_STAGE);

    Game *g = &s->game[next];
    g->wave.lift = FORM_AWAY;
    wave_hold_entries(&g->wave, true);
    wave_pause_attacks(&g->wave, true);

    /* Nothing dives until the formation is whole again. The latch that holds
       attacks back until then is tripped once per wave, and this board has
       already tripped it - so it is re-armed, and the arriving formation gets
       to finish arriving before anything comes out of it. */
    wave_rearm_attacks(&g->wave);

    s->phase = TURN_ARRIVING;
    session_log(s, "gone - next board announced");
}

static void session_tick(Session *s, const Input *in, bool to_title,
                         View *view, int *menu_sel)
{
    Game *cur = &s->game[s->turn];
    session_sync(s);
    ++s->tick;

    switch (s->phase) {
    case TURN_SETTLING:
        /* The outgoing board keeps running - the explosion has to play and the
           divers have to get home - but nothing new goes out to meet them, or
           the air would never clear. The player is already gone, so the input
           handed on is nobody's.

           A finished game will not run itself. game_update returns at once
           once `finished` is set, so calling it here advances nothing: no
           wave, no effects, and therefore a board that never settles. That is
           a hang, and it was one - a capture takes the last fighter without
           ending the turn, so the hand-over began *after* the game had
           finished, with a captor still flying home and nothing left to move
           it. What still has to finish is stepped directly instead. */
        if (cur->finished) {
            game_background_update();
            fx_update(&cur->fx);
            wave_update(&cur->wave, cur->player.x);
        } else {
            game_update(cur, in);
        }

        /* And a bounded wait regardless. Settling is the game waiting on
           itself, which is the one shape of wait that can fail to end; a
           handover that takes a moment too long is a blemish, one that never
           comes is the game stopping. */
        if (game_turn_settled(cur)) {
            session_log(s, "settled - the board leaves");
            s->settling = 0;
            s->phase    = TURN_LEAVING;
        } else if (++s->settling > SETTLE_LIMIT) {
            session_log(s, "GAVE UP waiting to settle - the board leaves anyway");
            if (cur->trace) wave_print_unsettled(&cur->wave);
            s->settling = 0;
            s->phase    = TURN_LEAVING;
        }
        return;

    case TURN_LEAVING:
        game_background_update();
        fx_update(&cur->fx);

        /* The wave still has to be stepped. The lift is an offset applied when
           a parked enemy's position is worked out, and that only happens
           inside wave_update - move the offset without stepping the wave and
           the number changes while the formation sits exactly where it was.
           Entries stay held and attacks are already paused, so stepping it
           here does nothing except carry the block upwards. */
        wave_update(&cur->wave, cur->player.x);

        if (wave_lift(&cur->wave, FORM_AWAY)) {
            /* A player whose crew is gone gets their numbers now, on the empty
               sky their board has just left, and before anybody else plays.
               Showing them later meant one player's results turning up in the
               middle of the other player's turn. */
            if (cur->over) {
                game_show_results(cur);
                s->phase = TURN_RESULTS;
                session_log(s, "gone - results for this player");
                return;
            }
            session_hand_to(s, s->incoming);
        }
        return;

    case TURN_RESULTS:
        /* game_update runs the results screen and sets `finished` at the end
           of it; the board is already away, so nothing else is moving. */
        game_update(cur, in);
        if (cur->finished) {
            int next = seat_next(s);
            if (next < 0) {
                /* Everybody is out. Ended here rather than by falling through
                   to the play code, which would have run another update and
                   read this seat's `over` flag a second time - logging a
                   hand-over that was not happening. */
                if (to_title) { *view = VIEW_TITLE; *menu_sel = MENU_ONE; }
                else          session_begin(s, s->seats);
                s->phase = TURN_PLAYING;
                return;
            }
            session_hand_to(s, next);
        }
        return;

    case TURN_ARRIVING: {
        Game *in_seat = &s->game[s->turn];
        game_background_update();

        /* Nothing of this board attacks while it is still arriving. */
        wave_pause_attacks(&in_seat->wave, true);

        if (s->handover > 0) { --s->handover; return; }

        wave_update(&in_seat->wave, in_seat->player.x);

        /* Whatever this seat had parked flies back down as a block; whatever
           had not launched yet resumes its own entry once it is home. */
        if (wave_lift(&in_seat->wave, 0.0f)) {
            wave_hold_entries(&in_seat->wave, false);
            in_seat->turn_over = false;
            s->phase = TURN_PLAYING;
            session_log(s, "arrived - controls live");
        }
        return;
    }

    default:
        break;
    }

    game_update(cur, in);

    /* A turn ends when a fighter is lost. With one seat that means nothing and
       the flag is simply cleared. With two it starts the sequence above. */
    if (cur->turn_over || cur->over) {
        /* A player who is out still has their board packed away and their
           numbers shown, so `over` starts the sequence even when there is
           nobody to hand to afterwards. */
        int next = cur->over ? s->turn : seat_next(s);
        if (!cur->over && (next < 0 || next == s->turn)) {
            cur->turn_over = false;
        } else {
            s->incoming = next;
            s->phase    = TURN_SETTLING;
            s->settling = 0;

            /* Nothing else goes out. Entries are held, and attacks are stopped
               too - wave_pause_attacks is otherwise only reached on the live
               path, which stops running the moment a game is over, so a board
               packing itself away carried on launching dives at a player who
               was no longer there and could never go quiet. */
            wave_hold_entries(&cur->wave, true);
            wave_pause_attacks(&cur->wave, true);
            session_log(s, "fighter lost - settling");
        }
    }

    /* The session is over only when *every* seat is - one player finishing
       leaves the other playing on alone, which is most of the point of taking
       turns. Deciding that here rather than inside the per-seat update is what
       makes that possible: the seat knows it is finished, the session knows
       whether that matters.

       A headless fast-forward starts a fresh session rather than stopping on
       the menu, since there is nobody to show a menu to. */
    if (seat_next(s) < 0) {
        if (to_title) { *view = VIEW_TITLE; *menu_sel = MENU_ONE; }
        else          session_begin(s, s->seats);
    }
}

/* ---------------------------------------------------------------- demo */

/* How long the title waits before showing the game off, and how long it shows
   it for. An arcade cabinet alternated the two forever; the only difference
   here is that the menu is a real menu, so the demo has to get out of the way
   the moment anybody touches anything. */
#define IDLE_BEFORE_DEMO 600     /* ten seconds of nobody home */
#define DEMO_LENGTH     1500     /* twenty-five seconds of playing */

/* The hands on the controls during a demo, and during a --at warm-up. One
   function so the game the harness measures and the game the attract screen
   shows are the same game being played the same way.

   It sweeps rather than sitting still: parked in the middle it would only ever
   shoot up one column, and a wave that never clears is a poor advertisement
   for the game and a useless warm-up. */
static Input demo_input(int tick)
{
    Input in = { false, false, true };
    int leftward = (tick / 90) % 2;
    in.left  = leftward != 0;
    in.right = leftward == 0;
    return in;
}

/* The game, with a word over it saying that nobody is playing. Without it a
   demonstration is indistinguishable from a game somebody abandoned. */
static void demo_draw(Gfx *g, const Game *game, int tick)
{
    game_draw(g, game);

    /* Low, in the band between the formation and the fighter's row. Centred
       vertically it sat across the bees, which is the part of the screen the
       demonstration exists to show. */
    if ((tick / 30) % 2 == 0) {
        const char *m = "DEMO";
        font_draw_scaled(g, (GAME_W - font_width_scaled(m, 2.0f)) / 2,
                         186.0f, YELLOW, m, 2.0f);
    }
    const char *k = "PRESS ANY KEY";
    font_draw(g, (GAME_W - font_width(k)) / 2, 206, DIM, k);
}

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

    static const char *ITEMS[MENU_COUNT] = { "1 PLAYER", "2 PLAYERS",
                                             "OPTIONS", "QUIT" };
    for (int i = 0; i < MENU_COUNT; ++i) {
        float y = 156.0f + i * 19.0f;
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
    if (TOOLS) font_draw(g, 4, GAME_H - 9, DIM, "TAB TOOLS");
}

/* The rows of the options page, in the order they are drawn. */
typedef enum { OPT_SFX, OPT_MUSIC, OPT_FULLSCREEN, OPT_COUNT } OptRow;

/* A level as a row of blocks rather than a number. Ten of them, lit up to the
   value: a bar can be read at a glance from across a room, which is the whole
   point of a setting you are adjusting by ear while it plays. */
static void draw_bar(Gfx *g, float x, float y, int value, bool on)
{
    const float W = 7.0f, H = 7.0f, GAP = 2.0f;
    for (int i = 0; i < VOLUME_STEPS; ++i) {
        float bx = x + i * (W + GAP);
        Vec2 quad[4] = {
            { bx,     y     }, { bx + W, y     },
            { bx + W, y + H }, { bx,     y + H },
        };
        SDL_Color c;
        if (i < value) c = on ? YELLOW : DIM;
        else           c = (SDL_Color){ 48, 48, 60, 255 };
        shape_draw_poly(g, quad, 4, c);
    }
}

static void options_draw(Gfx *g, const Settings *set, int sel)
{
    game_background_draw(g);
    const char *h = "OPTIONS";
    font_draw_scaled(g, (GAME_W - font_width_scaled(h, 2.0f)) / 2, 50.0f,
                     YELLOW, h, 2.0f);

    /* What is plugged in is worth saying: "does it see my controller" is the
       first question anyone asks, and a menu that cannot answer it sends them
       to the game to find out. */
    char buf[48];
    int pads = input_pads();
    if (pads > 0) {
        snprintf(buf, sizeof buf, "CONTROLLER  %d CONNECTED", pads);
    } else {
        snprintf(buf, sizeof buf, "CONTROLLER  NONE");
    }
    font_draw(g, (GAME_W - font_width(buf)) / 2, 154, pads > 0 ? CYAN : DIM, buf);

    /* The three settings. The cursor is the fighter, as it is on the menu -
       one cursor for the whole game rather than a second idea of what selected
       looks like. */
    const int LABEL_X = 34, VALUE_X = 116;
    for (int row = 0; row < OPT_COUNT; ++row) {
        int   y  = 84 + row * 16;
        bool  on = (row == sel);
        SDL_Color c = on ? YELLOW : DIM;

        static const char *NAMES[OPT_COUNT] = { "SOUND", "MUSIC", "FULL SCREEN" };
        font_draw(g, LABEL_X, y, c, NAMES[row]);

        if (row == OPT_FULLSCREEN) {
            font_draw(g, VALUE_X, y, on ? PALE : DIM,
                      set->fullscreen ? "ON" : "OFF");
        } else {
            draw_bar(g, (float)VALUE_X, (float)y - 1.0f,
                     row == OPT_SFX ? set->sfx : set->music, on);
        }

        if (on) {
            Vec2 cur = { (float)LABEL_X - 12.0f, (float)y + FONT_H * 0.5f };
            shape_draw(g, SHP_FIGHTER, cur, HEADING_E, 0.85f);
        }
    }

    const char *hint = "LEFT RIGHT TO CHANGE";
    font_draw(g, (GAME_W - font_width(hint)) / 2, 138, DIM, hint);

    /* The controls as three columns rather than two centred sentences. The
       sentences were 38 characters, and 38 characters at a six pixel advance
       is 228 - four pixels wider than the screen, so both ends were clipped.
       Columns keep every string short enough that no arrangement of them can
       run off the edge, and they line up, which a pair of separately centred
       lines never does. */
    const int COL_WHO = 30, COL_FLY = 72, COL_FIRE = 166;

    font_draw(g, COL_FLY,  168, DIM, "FLY");
    font_draw(g, COL_FIRE, 168, DIM, "FIRE");

    font_draw(g, COL_WHO,  182, CYAN, "PAD");
    font_draw(g, COL_FLY,  182, PALE, "STICK OR DPAD");
    font_draw(g, COL_FIRE, 182, PALE, "A");

    font_draw(g, COL_WHO,  194, CYAN, "KEYS");
    font_draw(g, COL_FLY,  194, PALE, "ARROWS");
    font_draw(g, COL_FIRE, 194, PALE, "SPACE");

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

static void usage(void)
{
    fprintf(stderr,
            "usage: claudaga [--title] [--scene] [--shapes] [--pose]\n"
            "                [--subject N] [--scale N]\n"
            "                [--at TICK] [--paths] [--observe] [--autofire]\n"
            "                [--trace] [--shot out.bmp] [--stats N]\n"
            "                [--stage N] [--mute] [--padtest] [--options]\n"
            "                [--audiotest [DIR]] [--paused] [--divedump] [--demo]\n"
            "                [--players 1|2] [--seed N]\n"
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

    int  opt_sel;      /* which row of the options page                   */
    bool paused;       /* the game is up but not running                  */

    /* The attract cycle. `idle` counts frames since anybody last touched a
       control on the title; `demo` counts down the demonstration itself. */
    int  idle;
    int  demo;

    /* Where Tab came from, so it can go back there. */
    View tab_home;

    /* Not owned here, but every menu action needs them. Passing them through
       each handler instead would mean five signatures changing every time a
       setting is added. */
    Settings *set;
    Gfx      *gfx;
    Session  *sess;
} Ui;

/* Everything a changed setting has to touch. Called from one place so that a
   setting cannot be applied in one direction and forgotten in the other -
   loading at startup and adjusting on the page go through the same code. */
static void ui_apply_settings(Ui *u)
{
    audio_set_levels(u->set->sfx, u->set->music);
    if (u->gfx) gfx_set_fullscreen(u->gfx, u->set->fullscreen);
    settings_save(u->set);
}

static void ui_up(Ui *u)
{
    if (u->options)      u->opt_sel  = (u->opt_sel + OPT_COUNT - 1) % OPT_COUNT;
    else if (u->view == VIEW_TITLE) {
        u->menu_sel = (u->menu_sel + MENU_COUNT - 1) % MENU_COUNT;
    }
}

static void ui_down(Ui *u)
{
    if (u->options)      u->opt_sel  = (u->opt_sel + 1) % OPT_COUNT;
    else if (u->view == VIEW_TITLE) {
        u->menu_sel = (u->menu_sel + 1) % MENU_COUNT;
    }
}

/* Left and right change the selected setting, and mean nothing anywhere else
   except the pose tool, which uses them to pick a shape. */
static void ui_adjust(Ui *u, int delta)
{
    if (u->options) {
        switch (u->opt_sel) {
        case OPT_SFX:
            u->set->sfx = u->set->sfx + delta;
            if (u->set->sfx < 0)            u->set->sfx = 0;
            if (u->set->sfx > VOLUME_STEPS) u->set->sfx = VOLUME_STEPS;
            break;
        case OPT_MUSIC:
            u->set->music = u->set->music + delta;
            if (u->set->music < 0)            u->set->music = 0;
            if (u->set->music > VOLUME_STEPS) u->set->music = VOLUME_STEPS;
            break;
        case OPT_FULLSCREEN:
            u->set->fullscreen = !u->set->fullscreen;
            break;
        default: break;
        }
        ui_apply_settings(u);
        return;
    }

    if (u->view == VIEW_POSE) {
        int n = (int)ARRAY_COUNT(POSE_SUBJECTS);
        u->subject = (u->subject + n + (delta > 0 ? 1 : -1)) % n;
    }
}

/* Pause only means anything with a game on screen. Sound stops with it: a
   paused game that keeps playing its music reads as one that has hung. */
static void ui_toggle_pause(Ui *u)
{
    if (u->view != VIEW_PLAY || u->options) return;
    u->paused = !u->paused;
    audio_pause(u->paused);
}

static void ui_confirm(Ui *u)
{
    /* On the options page the confirm button flips whatever is selected,
       which is what a player expects of a row that reads ON or OFF. */
    if (u->options) {
        if (u->opt_sel == OPT_FULLSCREEN) ui_adjust(u, 1);
        return;
    }
    if (u->view != VIEW_TITLE) return;

    if (u->menu_sel == MENU_ONE || u->menu_sel == MENU_TWO) {
        session_begin(u->sess, u->menu_sel == MENU_ONE ? 1 : 2);
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
    if (u->options) {
        u->options = false;
    } else if (u->view != VIEW_TITLE) {
        /* Leaving a paused game unpauses it, or the next one starts frozen
           with nothing on screen to say why. */
        if (u->paused) { u->paused = false; audio_pause(false); }
        u->view = VIEW_TITLE;
    }
}

/* Anything at all from a person cancels the attract cycle and resets the wait.
   It is deliberately not "a key that means something": on an arcade cabinet
   the demo stops the instant a coin goes in, and here the equivalent is any
   sign of life whatsoever. */
static void ui_awake(Ui *u, Game *game)
{
    u->idle = 0;
    if (u->demo > 0) {
        u->demo = 0;
        u->view = VIEW_TITLE;
        game->demo = false;
        audio_music_stop();
    }
}

/* Hands the game to itself. It restarts from stage one so the demonstration
   always shows the game from the beginning rather than from wherever the last
   person left it. */
static void ui_start_demo(Ui *u, Game *game)
{
    game->demo = false;      /* cleared first so the restart is a normal one */
    game_restart(game);
    game->demo = true;
    u->demo    = DEMO_LENGTH;
    u->view    = VIEW_DEMO;
    u->paused  = false;
    SDL_Log("attract: nobody home, showing the game off");
}

/* Tab visits the tools and comes back where it started.
 *
 * It used to walk the whole view list, which from the title screen meant its
 * first stop was the game - so the key the title screen labels TAB TOOLS
 * dropped you into a wave instead. The tools are the shape browser and the
 * pose check; play and the title are places to return to, not stations on the
 * way round. */
static void ui_next_view(Ui *u)
{
    if (!TOOLS || u->options) return;

    switch (u->view) {
    case VIEW_SHAPES: u->view = VIEW_POSE;    break;
    case VIEW_POSE:   u->view = u->tab_home;  break;
    default:
        u->tab_home = u->view;
        u->view     = VIEW_SHAPES;
        break;
    }
}

int main(int argc, char **argv)
{
    const char *shot_path = NULL;
    Ui          ui        = { VIEW_TITLE, MENU_ONE, false, true, 0,
                              OPT_SFX, false, 0, 0, VIEW_TITLE, NULL, NULL };
    Settings    settings;
    bool        view_set  = false;
    int         scale     = 3;
    int         warmup    = 0;
    int         stats     = 0;
    bool        paths     = false;
    bool        observe   = false;
    int         first_stage = 1;
    bool        mute      = false;
    bool        padtest   = false;
    bool        divedump  = false;
    bool        show_options = false;
    bool        show_paused  = false;
    bool        show_demo    = false;
    int         seats        = 1;
    const char *audiodir  = NULL;
    bool        audioreport = false;
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
        else if (!strcmp(argv[i], "--seed")    && i + 1 < argc)  wave_set_seed((unsigned)atoi(argv[++i]));
        else if (!strcmp(argv[i], "--stage")   && i + 1 < argc)  first_stage = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--paths"))                    paths = true;
        else if (!strcmp(argv[i], "--observe"))                  observe = true;
        else if (!strcmp(argv[i], "--autofire"))                 autofire = true;
        else if (!strcmp(argv[i], "--trace"))                    trace = true;
        else if (!strcmp(argv[i], "--mute"))                     mute = true;
        else if (!strcmp(argv[i], "--padtest"))                  padtest = true;
        else if (!strcmp(argv[i], "--divedump"))                 divedump = true;
        else if (!strcmp(argv[i], "--audiotest")) {
            audioreport = true;
            if (i + 1 < argc && argv[i + 1][0] != 0x2D) audiodir = argv[++i];
        }
        else if (!strcmp(argv[i], "--options")) { show_options = true; view_set = true; }
        else if (!strcmp(argv[i], "--paused"))  { show_paused  = true; view_set = true; }
        else if (!strcmp(argv[i], "--demo"))    { show_demo    = true; view_set = true; }
        else if (!strcmp(argv[i], "--players") && i + 1 < argc) seats = atoi(argv[++i]);
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
    if (show_paused)  { ui.view = VIEW_PLAY;  ui.paused  = true; }

    if (!view_set && (warmup > 0 || stats > 0)) ui.view = VIEW_PLAY;
    if (ui.subject < 0 || ui.subject >= ARRAY_COUNT(POSE_SUBJECTS)) ui.subject = 0;

    /* Before the window: the self test wants SDL up but has nothing to draw,
       and opening a window it would immediately close is noise. */
    if (divedump) { wave_dump_dives(); return 0; }

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
    /* Loaded before the device is opened so the first sound already plays at
       the chosen level, rather than at the default for a frame or two. */
    /* A fresh install gets its defaults written out rather than left implicit,
       so the file is there to be edited and the save path is exercised by
       every first run rather than only by someone opening the options page. */
    if (!settings_load(&settings)) settings_save(&settings);
    ui.set = &settings;
    ui.gfx = &g;

    audio_init(!mute && (audioreport || (!shot_path && stats <= 0)));
    audio_set_levels(settings.sfx, settings.music);

    /* Headless runs stay in a window whatever the file says: a screenshot run
       that took over the display would be a surprising thing for a build
       script to do. */
    if (!shot_path && stats <= 0) gfx_set_fullscreen(&g, settings.fullscreen);

    if (audioreport) {
        int bad = audio_report(audiodir);
        audio_shutdown();
        gfx_shutdown(&g);
        return bad;
    }

    /* Controllers are opened even for a headless run. The warm-up drives the
       game from a struct it fills in itself and never reads a pad, but a run
       that fast-forwards and then hands over wants one working when it does. */
    input_open();

    /* Several hundred KB of baked paths each; not stack-sized. Both exist
       whether or not two people are playing - a second seat that is only
       sometimes allocated is a second code path for everything that touches
       one. */
    static Game games[SEATS];
    for (int i = 0; i < SEATS; ++i) game_init(&games[i]);

    if (seats < 1) seats = 1;
    if (seats > SEATS) seats = SEATS;

    Session session = { games, 1, 0, 0 };
    ui.sess = &session;
    session_begin(&session, seats);

    Game *game = &games[0];   /* the seat the warm-up and the demo use */

    /* The demo needs a game to have been built, so it is set up here rather
       than beside the other view flags above. */
    if (show_demo) ui_start_demo(&ui, game);

    /* Starting later is a measurement tool rather than a cheat: the difficulty
       ramp only shows itself over a dozen stages, and playing up to stage 12 to
       check a number is not a test anybody runs twice. */
    if (first_stage > 1) {
        for (int i = 0; i < SEATS; ++i) {
            games[i].first_stage = first_stage;
            games[i].quiet = true;
            game_restart(&games[i]);
            games[i].quiet = false;
        }
    }

    /* Every seat, not just the first. Setting them on one meant a two-player
       run traced only half its own handovers, which looked like the turns not
       alternating when they were. */
    for (int i = 0; i < SEATS; ++i) {
        games[i].wave.show_paths = paths;
        games[i].invulnerable    = observe;
        games[i].trace           = trace;
    }

    /* --stats exercises the wave on its own rather than the whole game: who
       attacks and how often is a property of the wave, and letting the fighter
       die mid-run would restart it and skew the tally. */
    if (stats > 0) {
        for (int i = 0; i < stats; ++i) wave_update(&game->wave, game->player.x);
        wave_print_stats(&game->wave);
        input_close();
        audio_shutdown();
        gfx_shutdown(&g);
        return 0;
    }

    /* --at runs the simulation forward with no rendering, so a screenshot can
       be taken at a chosen moment rather than only at tick 0. Pair it with
       --observe to stop the fighter dying and restarting the run. */
    for (int i = 0; i < warmup; ++i) {
        Input warm = autofire ? demo_input(i) : (Input){ false, false, false };
        session_tick(&session, &warm, WARMUP_TO_TITLE, &ui.view, &ui.menu_sel);
    }

    /* The closest two enemies ever came while flying the same entry path, per
       seat. --stats reports this for a wave driven on its own, but it cannot
       drive a session, and a session is where it goes wrong: handing a board
       away and bringing it back is exactly the sort of thing that stacks an
       entry. Two players read 0.0px here before the launch schedule was taught
       to wait along with the hold. */
    if (trace && warmup > 0) {
        for (int i = 0; i < SEATS; ++i) {
            float gap = games[i].wave.min_lane_gap;
            if (gap > 1e8f) continue;          /* that seat never played */
            printf("seat %d: closest two enemies on one path %.1f px\n",
                   i + 1, gap);
        }
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

            if (ev.type == SDL_KEYDOWN || ev.type == SDL_CONTROLLERBUTTONDOWN ||
                ev.type == SDL_MOUSEBUTTONDOWN) {
                ui_awake(&ui, game);
            }

            if (ev.type == SDL_QUIT) {
                ui.running = false;
            } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE:      ui_back(&ui);            break;
                case SDLK_TAB:         ui_next_view(&ui);       break;
                case SDLK_UP:          ui_up(&ui);              break;
                case SDLK_DOWN:        ui_down(&ui);            break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if (ev.key.keysym.mod & KMOD_ALT) {
                        ui.set->fullscreen = !ui.set->fullscreen;
                        ui_apply_settings(&ui);
                    } else {
                        ui_confirm(&ui);
                    }
                    break;
                case SDLK_LEFT:
                case SDLK_PAGEUP:      ui_adjust(&ui, -1);      break;
                case SDLK_RIGHT:
                case SDLK_PAGEDOWN:    ui_adjust(&ui, +1);      break;

                /* P is the pause key everyone reaches for, so it is the
                   player's; the path overlay it used to hold has moved to a
                   function key with the other debug toggles. */
                case SDLK_p:           ui_toggle_pause(&ui);    break;

                case SDLK_F11:
                    ui.set->fullscreen = !ui.set->fullscreen;
                    ui_apply_settings(&ui);
                    break;

                case SDLK_r:
                    if (TOOLS && ui.view == VIEW_PLAY) {
                        session_begin(&session, session.seats);
                    }
                    break;
                case SDLK_F2:
                    if (TOOLS) {
                        games[session.turn].wave.show_paths =
                            !games[session.turn].wave.show_paths;
                    }
                    break;
                case SDLK_F3:
                    if (TOOLS) {
                        games[session.turn].wave.attacks_enabled =
                            !games[session.turn].wave.attacks_enabled;
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
            ui_awake(&ui, game);
            switch (a) {
            case UI_UP:        ui_up(&ui);             break;
            case UI_DOWN:      ui_down(&ui);           break;
            case UI_LEFT:      ui_adjust(&ui, -1);     break;
            case UI_RIGHT:     ui_adjust(&ui, +1);     break;
            case UI_PAUSE:     ui_toggle_pause(&ui);   break;
            case UI_MENU:      ui_back(&ui);           break;
            /* Nothing a thumb rests on does anything during play. A and B
               are the fire buttons; pausing is Start and leaving is Back. */
            case UI_CONFIRM:
                if (ui.view != VIEW_PLAY || ui.options) ui_confirm(&ui);
                break;
            case UI_BACK:
                if (ui.view != VIEW_PLAY || ui.options) ui_back(&ui);
                break;
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
                /* A paused game is drawn but not stepped - including the
                   starfield, since stars drifting behind a frozen board is
                   exactly what a hang looks like. */
                if (!ui.paused) {
                    session_tick(&session, &in, true, &ui.view, &ui.menu_sel);
                }
            } else if (ui.view == VIEW_DEMO) {
                Input hands = demo_input(tick);
                game_update(game, &hands);

                /* It ends when its time is up or when it loses, whichever
                   comes first - a demonstration that sat on a results screen
                   would be showing the one part of the game nobody needs
                   advertising. */
                if (--ui.demo <= 0 || game->finished || game->results > 0) {
                    SDL_Log("attract: demo over after %d ticks%s",
                            DEMO_LENGTH - ui.demo,
                            game->finished || game->results > 0 ? " (lost)" : "");
                    ui.demo    = 0;
                    ui.idle    = 0;
                    game->demo = false;
                    ui.view    = VIEW_TITLE;
                }
            } else {
                game_background_update();

                /* Nobody home for long enough, and the game starts showing
                   itself off. Only from the title proper: the options page and
                   the shape tools are places somebody is deliberately looking
                   at something. */
                if (ui.view == VIEW_TITLE && !ui.options) {
                    if (++ui.idle >= IDLE_BEFORE_DEMO) ui_start_demo(&ui, game);
                } else {
                    ui.idle = 0;
                }

                /* Everything that is not the game runs under the title music -
                   the menu, the options page and both shape tools. Asking for
                   a track already playing does nothing, so this needs no state
                   of its own. */
                audio_music(MUSIC_TITLE);
            }
        }

        gfx_begin_frame(&g);
        if (ui.options)               options_draw(&g, &settings, ui.opt_sel);
        else if (ui.view == VIEW_TITLE)  title_draw(&g, ui.menu_sel, tick);
        else if (ui.view == VIEW_PLAY)   game_draw(&g, &games[session.turn]);
        else if (ui.view == VIEW_DEMO)   demo_draw(&g, game, tick);
        else if (ui.view == VIEW_POSE)   pose_draw(&g, ui.subject);
        else                          shapes_draw(&g, tick);

        /* Whose turn it is now, over their own frozen board. */
        if (session.handover > 0 && ui.view == VIEW_PLAY && !ui.options) {
            char who[24];
            snprintf(who, sizeof who, "PLAYER %d", session.turn + 1);
            font_draw_scaled(&g, (GAME_W - font_width_scaled(who, 2.0f)) / 2,
                             GAME_H * 0.5f - 8.0f, YELLOW, who, 2.0f);
            const char *r = "READY";
            font_draw(&g, (GAME_W - font_width(r)) / 2, GAME_H / 2 + 14, CYAN, r);
        }

        /* Over the frozen board rather than instead of it: seeing where
           everything stopped is most of the reason to pause. */
        if (ui.paused && ui.view == VIEW_PLAY && !ui.options) {
            const char *m = "PAUSED";
            font_draw_scaled(&g, (GAME_W - font_width_scaled(m, 2.0f)) / 2,
                             GAME_H * 0.5f - 8.0f, YELLOW, m, 2.0f);
            const char *k = "P TO RESUME   ESC FOR MENU";
            font_draw(&g, (GAME_W - font_width(k)) / 2, GAME_H / 2 + 14, DIM, k);
        }

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
