#ifndef CLAUDAGA_FONT_H
#define CLAUDAGA_FONT_H

#include "gfx.h"

/* A stroke font: every glyph is a handful of line segments, drawn with real
 * thickness as geometry.
 *
 * It replaces a 5x7 bitmap, which scaled by turning each pixel into a bigger
 * square and looked increasingly out of place beside artwork that had gone
 * smooth. Strokes scale the way the ships do.
 *
 * The metrics are deliberately the ones the bitmap had, so every existing
 * caller and every piece of layout arithmetic still lines up: a glyph occupies
 * a box FONT_W across and FONT_H down from the position given, and glyphs step
 * by FONT_ADVANCE. Segment coordinates are authored on that same box. */

#define FONT_W       5
#define FONT_H       7
#define FONT_ADVANCE (FONT_W + 1)

void font_draw(Gfx *g, int x, int y, SDL_Color c, const char *text);
int  font_width(const char *text);   /* pixel width, excluding trailing gap */

/* Same, scaled about the top-left of the text. Used where a message wants to
   be bigger than the HUD without a second set of glyphs. */
void font_draw_scaled(Gfx *g, float x, float y, SDL_Color c, const char *text,
                      float scale);
float font_width_scaled(const char *text, float scale);

#endif /* CLAUDAGA_FONT_H */
