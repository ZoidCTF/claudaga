#ifndef CLAUDAGA_GFX_H
#define CLAUDAGA_GFX_H

#include <SDL.h>
#include "common.h"

/* Nothing here loads an image. Every pixel the game puts on screen is
   generated: the artwork is polygons, the text is strokes, the explosions and
   the tractor beam are built per frame. */
typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
} Gfx;

/* Opens the window and renderer. Returns false and prints the reason on
   failure. `scale` is the initial integer zoom of the 224x288 picture. */
bool gfx_init(Gfx *g, const char *title, int scale);
void gfx_shutdown(Gfx *g);

/* Grabs the game area (letterbox bars excluded) into a .bmp. Call it after
   drawing but before gfx_end_frame - once the frame is presented the back
   buffer's contents are not guaranteed to survive. */
bool gfx_screenshot(Gfx *g, const char *path);

/* Borderless fullscreen, and nothing else. The renderer already scales the
   224x288 picture to whatever size the window is, so there is no resolution to
   choose and no mode to switch: the desktop's own is always the right one.
   Exclusive fullscreen would buy nothing here and cost alt-tab behaviour. */
void gfx_set_fullscreen(Gfx *g, bool on);
bool gfx_is_fullscreen(const Gfx *g);

void gfx_begin_frame(Gfx *g);
void gfx_end_frame(Gfx *g);

#endif /* CLAUDAGA_GFX_H */
