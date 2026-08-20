#ifndef CLAUDAGA_INPUT_H
#define CLAUDAGA_INPUT_H

#include <stdbool.h>

#include <SDL.h>

/* Keyboard and game controllers, merged.
 *
 * The game does not read a keyboard. It reads three booleans, and this file is
 * the only thing that knows where they came from - which is what stops "and
 * now the same for a controller" from meaning a second `if` beside every
 * existing one. It is also what lets the headless harness drive the game by
 * filling in the struct directly instead of forging a scancode array.
 *
 * Two kinds of input live here and they are not interchangeable. Flying is
 * sampled: what matters is whether left is held right now. Menus are
 * edge-triggered: holding a direction should move the cursor once, not sixty
 * times a second. The keyboard gets its edges free from SDL_KEYDOWN, but a
 * stick pushed to one side is a level rather than an event, so those edges
 * have to be manufactured - which is what the queue below is for. */

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
    UI_CONFIRM,
    UI_BACK,
    UI_NEXT_VIEW
} UiAction;

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
