#ifndef CLAUDAGA_ICON_H
#define CLAUDAGA_ICON_H

#include "gfx.h"

/* The application icon, drawn from the same polygons as the fighter that flies
   in the game, so it cannot drift from the artwork.
 *
 * It has to exist as a file before the exe can link it, so res/claudaga.ico is
 * committed and regenerated on demand:
 *
 *     claudaga --icon res\claudaga.ico
 */

/* One size, as a fresh RGBA surface on transparency. Caller frees it. */
SDL_Surface *icon_render(Gfx *g, int size);

/* Every size Windows asks for, into a .ico. */
bool icon_write(Gfx *g, const char *path);

#endif /* CLAUDAGA_ICON_H */
