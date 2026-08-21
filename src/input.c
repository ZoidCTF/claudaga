#include "input.h"

#include <stdio.h>
#include <string.h>

/* How many pads to track at once. Four is the number of ports an arcade
   cabinet's worth of players would ever plausibly want, and every one of them
   drives the same fighter - there is no second player yet, so a second pad is
   simply another way to fly the first. */
#define MAX_PADS 4

/* Stick deadzone, out of 32767. A resting stick rarely reads zero, and without
   this the fighter drifts on its own - which looks like the game moving by
   itself rather than like a controller that needs recentring. */
#define STICK_DEADZONE 9000

/* Where the stick has to reach to count as a menu press, and where it has to
   fall back to before it can count again. The gap between the two is what
   stops a stick resting near the threshold from chattering the cursor up and
   down; a single threshold would do exactly that. */
#define MENU_PRESS   18000
#define MENU_RELEASE  9000

/* Menu actions are queued rather than acted on where they are noticed, because
   they arrive from two places - button events and axis sampling - at different
   points in the frame. Small: nobody presses eight things between frames, and
   dropping the ninth is better than growing a buffer for it. */
#define UI_QUEUE 8

static SDL_GameController *s_pad[MAX_PADS];
static SDL_JoystickID      s_pad_id[MAX_PADS];
static int                 s_pads;

static UiAction s_queue[UI_QUEUE];
static int      s_q_head, s_q_tail;

/* Which way each stick was last seen, so a crossing can be spotted. One per
   axis: a menu with rows and values needs both, and they arm independently -
   pushing up out of a left-held stick should still count. */
static int s_menu_dir_y[MAX_PADS];
static int s_menu_dir_x[MAX_PADS];

static void ui_push(UiAction a)
{
    int next = (s_q_tail + 1) % UI_QUEUE;
    if (next == s_q_head) return;   /* full; the oldest survives */
    s_queue[s_q_tail] = a;
    s_q_tail = next;
}

UiAction input_take_ui(void)
{
    if (s_q_head == s_q_tail) return UI_NONE;
    UiAction a = s_queue[s_q_head];
    s_q_head = (s_q_head + 1) % UI_QUEUE;
    return a;
}

static int pad_slot(SDL_JoystickID id)
{
    for (int i = 0; i < MAX_PADS; ++i) {
        if (s_pad[i] && s_pad_id[i] == id) return i;
    }
    return -1;
}

static void pad_add(int device_index)
{
    if (!SDL_IsGameController(device_index)) return;
    if (s_pads >= MAX_PADS) return;

    /* Refuse a device that is already open. Opening the subsystem enumerates
       what is plugged in, and SDL *also* queues a DEVICEADDED for each of
       those same devices - so without this every controller present at
       startup is opened twice. That is not merely a wrong count: the second
       handle is closed by the first unplug, leaving the first handle dangling
       and being read every frame. It cost three failures in the self test
       before it was found, and none of them looked like this. */
    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device_index);
    if (id >= 0 && pad_slot(id) >= 0) return;

    SDL_GameController *c = SDL_GameControllerOpen(device_index);
    if (!c) {
        SDL_Log("input: could not open controller %d (%s)",
                device_index, SDL_GetError());
        return;
    }

    SDL_Joystick *j = SDL_GameControllerGetJoystick(c);
    for (int i = 0; i < MAX_PADS; ++i) {
        if (s_pad[i]) continue;
        s_pad[i]      = c;
        s_pad_id[i]   = SDL_JoystickInstanceID(j);
        s_menu_dir_y[i] = 0;
        s_menu_dir_x[i] = 0;
        ++s_pads;
        SDL_Log("input: controller connected - %s",
                SDL_GameControllerName(c) ? SDL_GameControllerName(c) : "unnamed");
        return;
    }
    SDL_GameControllerClose(c);
}

static void pad_remove(SDL_JoystickID id)
{
    int i = pad_slot(id);
    if (i < 0) return;
    SDL_GameControllerClose(s_pad[i]);
    s_pad[i] = NULL;
    --s_pads;
    SDL_Log("input: controller disconnected");
}

void input_open(void)
{
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        SDL_Log("input: no controller subsystem (%s) - keyboard only",
                SDL_GetError());
        return;
    }
    for (int i = 0; i < SDL_NumJoysticks(); ++i) pad_add(i);
}

void input_close(void)
{
    for (int i = 0; i < MAX_PADS; ++i) {
        if (!s_pad[i]) continue;
        SDL_GameControllerClose(s_pad[i]);
        s_pad[i] = NULL;
    }
    s_pads   = 0;
    s_q_head = s_q_tail = 0;
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void input_event(const SDL_Event *ev)
{
    switch (ev->type) {
    case SDL_CONTROLLERDEVICEADDED:
        pad_add(ev->cdevice.which);
        break;

    case SDL_CONTROLLERDEVICEREMOVED:
        pad_remove(ev->cdevice.which);
        break;

    case SDL_CONTROLLERBUTTONDOWN:
        switch (ev->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    ui_push(UI_UP);      break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  ui_push(UI_DOWN);    break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  ui_push(UI_LEFT);    break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: ui_push(UI_RIGHT);   break;
        case SDL_CONTROLLER_BUTTON_A:          ui_push(UI_CONFIRM); break;
        case SDL_CONTROLLER_BUTTON_B:          ui_push(UI_BACK);    break;

        /* Start affirms on a menu and pauses in play. Both are pushed; which
           one means anything is the view's business, not this file's - input
           has no idea what a view is and should not learn. */
        case SDL_CONTROLLER_BUTTON_START:
            ui_push(UI_CONFIRM);
            ui_push(UI_PAUSE);
            break;

        case SDL_CONTROLLER_BUTTON_BACK:       ui_push(UI_MENU);    break;

        /* The shoulder buttons cycle the tools, which is what Tab does. */
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            ui_push(UI_NEXT_VIEW);
            break;
        default: break;
        }
        break;

    default: break;
    }
}

/* Fire is any of the face buttons that fall under the thumb, plus the right
   trigger. Spreading it about rather than insisting on one is worth doing:
   which button is "the" button is a matter of what somebody grew up holding,
   and none of the four is needed for anything else in flight. */
static bool pad_firing(SDL_GameController *c)
{
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A) ||
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B) ||
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X) ||
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_Y)) {
        return true;
    }
    return SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000;
}

void input_sample(Input *in)
{
    memset(in, 0, sizeof *in);

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if (keys) {
        in->left  = keys[SDL_SCANCODE_LEFT]  != 0;
        in->right = keys[SDL_SCANCODE_RIGHT] != 0;
        in->fire  = keys[SDL_SCANCODE_SPACE] != 0;
    }

    for (int i = 0; i < MAX_PADS; ++i) {
        SDL_GameController *c = s_pad[i];
        if (!c) continue;

        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  in->left  = true;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) in->right = true;

        int x = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
        if (x < -STICK_DEADZONE) in->left  = true;
        if (x >  STICK_DEADZONE) in->right = true;

        if (pad_firing(c)) in->fire = true;

        /* The stick's menu edges. Held one way it should move the cursor once,
           so an action is only queued on the crossing, and the stick has to
           come most of the way back before it can cross again. */
        int y = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
        int dy = s_menu_dir_y[i];
        if (dy == 0) {
            if (y < -MENU_PRESS) { dy = -1; ui_push(UI_UP);   }
            if (y >  MENU_PRESS) { dy =  1; ui_push(UI_DOWN); }
        } else if (y > -MENU_RELEASE && y < MENU_RELEASE) {
            dy = 0;
        }
        s_menu_dir_y[i] = dy;

        int dx = s_menu_dir_x[i];
        if (dx == 0) {
            if (x < -MENU_PRESS) { dx = -1; ui_push(UI_LEFT);  }
            if (x >  MENU_PRESS) { dx =  1; ui_push(UI_RIGHT); }
        } else if (x > -MENU_RELEASE && x < MENU_RELEASE) {
            dx = 0;
        }
        s_menu_dir_x[i] = dx;
    }

    /* Both at once cancels out, the way two arrow keys do. Left and right are
       separate flags rather than one axis precisely so that this is a decision
       taken here and not an accident of which was read last. */
    if (in->left && in->right) in->left = in->right = false;
}

int input_pads(void)
{
    return s_pads;
}

/* ------------------------------------------------------------- self test */

/* The virtual joystick SDL builds for us has the standard layout, so the axis
   and button numbers below are the SDL_CONTROLLER_* enumerators themselves. */
#define VPAD_AXES    6
#define VPAD_BUTTONS 15
#define VPAD_HATS    1

static int s_fails;

static void check(const char *what, bool got, bool want)
{
    bool ok = (got == want);
    if (!ok) ++s_fails;
    printf("  %-44s %s (got %d, want %d)\n",
           what, ok ? "ok  " : "FAIL", (int)got, (int)want);
}

/* Feeds SDL's queue through input_event the way the main loop does. */
static void pump(void)
{
    SDL_Event ev;
    SDL_PumpEvents();
    while (SDL_PollEvent(&ev)) input_event(&ev);
}

int input_selftest(void)
{
    s_fails = 0;
    printf("controller self test\n");

    input_open();
    int before = input_pads();

    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                          VPAD_AXES, VPAD_BUTTONS, VPAD_HATS);
    if (index < 0) {
        printf("  could not attach a virtual pad (%s)\n", SDL_GetError());
        input_close();
        return 1;
    }

    pump();
    check("a plugged-in pad is opened", input_pads() == before + 1, true);

    SDL_Joystick *vj = SDL_JoystickOpen(index);
    if (!vj) {
        printf("  could not open the virtual pad (%s)\n", SDL_GetError());
        input_close();
        return 1;
    }

    Input in;

    /* The stick, both ways, and the deadzone between them. */
    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_LEFTX, -20000);
    pump(); input_sample(&in);
    check("stick left gives left", in.left, true);
    check("stick left does not give right", in.right, false);

    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_LEFTX, 20000);
    pump(); input_sample(&in);
    check("stick right gives right", in.right, true);

    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_LEFTX, 3000);
    pump(); input_sample(&in);
    check("a stick inside the deadzone is still", in.left || in.right, false);

    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_LEFTX, 0);

    /* The d-pad, which is a button rather than an axis. */
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);
    pump(); input_sample(&in);
    check("dpad left gives left", in.left, true);
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0);

    /* Both directions at once cancel, as two arrow keys do. */
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1);
    pump(); input_sample(&in);
    check("left and right together cancel", in.left || in.right, false);
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0);
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0);

    /* Fire, on each of the face buttons and on the trigger. */
    static const int FIRE_BUTTONS[] = {
        SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
    };
    for (int i = 0; i < 4; ++i) {
        SDL_JoystickSetVirtualButton(vj, FIRE_BUTTONS[i], 1);
        pump(); input_sample(&in);
        check("a face button fires", in.fire, true);
        SDL_JoystickSetVirtualButton(vj, FIRE_BUTTONS[i], 0);
        while (input_take_ui() != UI_NONE) { }   /* those are menu presses too */
    }

    /* A trigger at rest is raw -32768, not raw 0. The mapping normalises a
       full-range axis on to 0..32767, so raw 0 comes out at 16383 - half
       pressed, which fires, and correctly so. Worth writing down: the first
       version of this test released the trigger to 0 and then reported the
       game as broken for doing exactly the right thing with it. */
    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 26000);
    pump(); input_sample(&in);
    check("the right trigger fires", in.fire, true);
    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    pump(); input_sample(&in);
    check("a released trigger stops firing", in.fire, false);

    /* Menu edges. This is the part that cannot be got right by accident: a
       held stick must move the cursor once, and only a return to centre may
       arm it again. */
    while (input_take_ui() != UI_NONE) { }

    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_LEFTY, -25000);
    pump(); input_sample(&in);
    check("stick up queues one menu press", input_take_ui() == UI_UP, true);
    check("and nothing behind it", input_take_ui() == UI_NONE, true);

    input_sample(&in);   /* still held */
    check("a held stick does not repeat", input_take_ui() == UI_NONE, true);

    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_LEFTY, 0);
    pump(); input_sample(&in);
    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_LEFTY, -25000);
    pump(); input_sample(&in);
    check("recentring arms it again", input_take_ui() == UI_UP, true);
    SDL_JoystickSetVirtualAxis(vj, SDL_CONTROLLER_AXIS_LEFTY, 0);
    pump(); input_sample(&in);
    while (input_take_ui() != UI_NONE) { }

    /* Buttons that drive the menu.
     *
     * The face buttons are fire, so what they must *not* do matters as much as
     * what they do. A pushing a confirm is fine on a menu and was a disaster in
     * play, where confirm meant pause and every shot toggled the game on and
     * off; B pushing a back abandoned the run. They still push those actions -
     * a menu needs them - and the view decides whether to listen. Pause and
     * leaving are on Start and Back, which no thumb rests on. */
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_A, 1);
    pump();
    check("A confirms", input_take_ui() == UI_CONFIRM, true);
    check("A does not pause", input_take_ui() == UI_NONE, true);
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_A, 0);
    pump(); while (input_take_ui() != UI_NONE) { }

    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_B, 1);
    pump();
    check("B cancels", input_take_ui() == UI_BACK, true);
    check("B does nothing else", input_take_ui() == UI_NONE, true);
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_B, 0);
    pump(); while (input_take_ui() != UI_NONE) { }

    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_START, 1);
    pump();
    check("start confirms", input_take_ui() == UI_CONFIRM, true);
    check("start also pauses", input_take_ui() == UI_PAUSE, true);
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_START, 0);
    pump(); while (input_take_ui() != UI_NONE) { }

    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_BACK, 1);
    pump();
    check("back asks for the menu", input_take_ui() == UI_MENU, true);
    SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_BACK, 0);
    pump(); while (input_take_ui() != UI_NONE) { }

    /* Unplugging. */
    SDL_JoystickClose(vj);
    SDL_JoystickDetachVirtual(index);
    pump();
    check("an unplugged pad is closed", input_pads() == before, true);

    input_sample(&in);
    check("nothing is held once it is gone", in.left || in.right || in.fire, false);

    input_close();

    printf("controller self test: %d failure(s)\n", s_fails);
    return s_fails;
}
