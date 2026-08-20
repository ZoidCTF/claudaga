#ifndef CLAUDAGA_GFX_H
#define CLAUDAGA_GFX_H

#include <SDL.h>
#include "common.h"

typedef struct {
    SDL_Texture *tex;
    int w, h;
} Texture;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    Texture       sheet;   /* the whole ripped sprite sheet, loaded once */
} Gfx;

/* Opens the window and renderer. Returns false and prints the reason on
   failure. `scale` is the initial integer zoom of the 224x288 picture. */
bool gfx_init(Gfx *g, const char *title, int scale);
void gfx_shutdown(Gfx *g);

/* Loads a PNG through stb_image. When `colorkey_black` is true every fully
   black pixel becomes transparent, which is how the arcade treated palette
   entry 0 and how the ripped sheet is laid out. */
bool gfx_load_texture(Gfx *g, Texture *out, const char *path, bool colorkey_black);
void gfx_free_texture(Texture *t);

/* Blits `src` from the sheet with its top-left corner at (x, y). */
void gfx_blit(Gfx *g, const SDL_Rect *src, int x, int y);

/* Same, but mirrored. The sprite sheet only stores one quadrant of each
   flyer's rotation, so drawing the other three means mirroring - which is what
   the arcade hardware did too. */
void gfx_blit_flip(Gfx *g, const SDL_Rect *src, int x, int y, SDL_RendererFlip flip);

/* Free rotation about the cell's centre. Not used for the flyers, whose angles
   come from the sheet's own frames, but handy for effects. */
void gfx_blit_rot(Gfx *g, const SDL_Rect *src, int x, int y, double angle);

/* Grabs the game area (letterbox bars excluded) into a .bmp. Call it after
   drawing but before gfx_end_frame - once the frame is presented the back
   buffer's contents are not guaranteed to survive. */
bool gfx_screenshot(Gfx *g, const char *path);

void gfx_begin_frame(Gfx *g);
void gfx_end_frame(Gfx *g);

#endif /* CLAUDAGA_GFX_H */
