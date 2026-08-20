#ifndef CLAUDAGA_DEBUGFONT_H
#define CLAUDAGA_DEBUGFONT_H

#include "gfx.h"

/* A 5x7 bitmap font used only by the debug overlays. The sprite sheet carries
   no alphabet - just the score digits - so labels need their own glyphs. When
   the real Galaga font gets ripped this can go away. */
#define FONT_W 5
#define FONT_H 7
#define FONT_ADVANCE (FONT_W + 1)

void font_draw(Gfx *g, int x, int y, SDL_Color c, const char *text);
int  font_width(const char *text);   /* pixel width, excluding trailing gap */

#endif /* CLAUDAGA_DEBUGFONT_H */
