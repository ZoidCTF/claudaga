#ifndef CLAUDAGA_INPUT_H
#define CLAUDAGA_INPUT_H

#include <stdbool.h>

#include <SDL.h>

/* Keyboard and game controllers, merged. The game reads three booleans and
 * this is the only file that knows where they came from, which is also what
 * lets the headless harness fill the struct in directly.
 *
 * Two kinds of input, not interchangeable. Flying is sampled - is left held
 * now. Menus are edge-triggered - holding a direction moves the cursor once,
 * not sixty times a second. SDL_KEYDOWN gives the keyboard those edges free,
 * but a stick held to one side is a level, so the queue below makes them. */

typedef struct {
    bool left, right;   /* held */
    bool fire;          /* held */
} Input;

/* Edge-triggered menu actions, from a controller. The keyboard equivalents
   arrive as ordinary key events and go through the same handlers. */
typedef enum {
    UI_NONE,
    UI_UP,
    UI_DOWN,
    UI_LEFT,
    UI_RIGHT,
    UI_CONFIRM,      /* A or Start - an affirmative, on a menu       */
    UI_BACK,         /* B - a cancel, on a menu                      */
    UI_PAUSE,        /* Start - and only Start                       */
    UI_MENU,         /* Back/Select - out of the game, from anywhere  */
    UI_NEXT_VIEW
} UiAction;

/* Why confirm and pause are separate actions rather than one button read two
   ways: A and B are *fire*. Firing must not also be pressing a menu button,
   and it was - A confirmed, which during play meant pause, so every shot
   toggled the game on and off; B went back, which during play meant abandoning
   the run. Play gets exactly one thing from the face buttons now, which is
   shooting, and the buttons that mean something else are the ones no thumb
   rests on. */

/* Opens the controller subsystem and every pad already plugged in. Safe to
   call when there are none, and safe to call when the subsystem will not start
   at all - the game carries on with the keyboard, which is the only input it
   is entitled to assume. */
void input_open(void);
void input_close(void);

/* Device arrival and departure, and controller buttons. Pass every event; it
   ignores what it does not care about. */
void input_event(const SDL_Event *ev);

/* Merges the keyboard and every open pad into `in`. */
void input_sample(Input *in);

/* Takes the next queued menu action, or UI_NONE when there is nothing. Drain
   this in a loop once a frame. */
UiAction input_take_ui(void);

/* How many controllers are open, for the options screen to report. */
int input_pads(void);

/* Drives a virtual controller through this file and checks what comes out the
   other side. Controller support is otherwise the one thing here that cannot
   be tested without somebody holding a pad: SDL can attach a synthetic one and
   have its axes and buttons set from code, which exercises the real open,
   event, sample and close paths rather than a mock of them.

   Returns the number of failures; prints a line per check. */
int input_selftest(void);

#endif /* CLAUDAGA_INPUT_H */
