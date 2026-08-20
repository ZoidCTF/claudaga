#include "font.h"

#include <math.h>
#include <string.h>

/* Glyphs are authored on a box 4 wide and 6 tall - one less than FONT_W and
   FONT_H, because these are coordinates of points rather than counts of
   pixels, and a 5-pixel-wide box has its far edge at x = 4. Four floats per
   segment: x0, y0, x1, y1. The style is deliberately squared off; curves cost
   segments and read no better at this size. */

#define G(name) static const float name[] =

G(SEG_A) { 0,6, 0,2,  0,2, 2,0,  2,0, 4,2,  4,2, 4,6,  0,4, 4,4 };
G(SEG_B) { 0,0, 0,6,  0,0, 3.4f,0,  3.4f,0, 3.4f,3,  0,3, 3.4f,3,
           3.4f,3, 3.4f,6,  0,6, 3.4f,6 };
G(SEG_C) { 4,0, 0,0,  0,0, 0,6,  0,6, 4,6 };
G(SEG_D) { 0,0, 0,6,  0,0, 3,0,  3,0, 4,1.4f,  4,1.4f, 4,4.6f,
           4,4.6f, 3,6,  0,6, 3,6 };
G(SEG_E) { 4,0, 0,0,  0,0, 0,6,  0,6, 4,6,  0,3, 3,3 };
G(SEG_F) { 4,0, 0,0,  0,0, 0,6,  0,3, 3,3 };
G(SEG_G) { 4,0, 0,0,  0,0, 0,6,  0,6, 4,6,  4,6, 4,3.4f,  4,3.4f, 2,3.4f };
G(SEG_H) { 0,0, 0,6,  4,0, 4,6,  0,3, 4,3 };
G(SEG_I) { 1,0, 3,0,  2,0, 2,6,  1,6, 3,6 };
G(SEG_J) { 4,0, 4,5,  4,5, 3,6,  3,6, 1,6,  1,6, 0,5 };
G(SEG_K) { 0,0, 0,6,  4,0, 0,3.2f,  0,3.2f, 4,6 };
G(SEG_L) { 0,0, 0,6,  0,6, 4,6 };
G(SEG_M) { 0,6, 0,0,  0,0, 2,2.6f,  2,2.6f, 4,0,  4,0, 4,6 };
G(SEG_N) { 0,6, 0,0,  0,0, 4,6,  4,6, 4,0 };
G(SEG_O) { 0,0, 4,0,  4,0, 4,6,  4,6, 0,6,  0,6, 0,0 };
G(SEG_P) { 0,6, 0,0,  0,0, 4,0,  4,0, 4,3,  4,3, 0,3 };
G(SEG_Q) { 0,0, 4,0,  4,0, 4,4.4f,  4,4.4f, 2.6f,6,  2.6f,6, 0,6,
           0,6, 0,0,  2.4f,4, 4,6 };
G(SEG_R) { 0,6, 0,0,  0,0, 4,0,  4,0, 4,3,  4,3, 0,3,  1.6f,3, 4,6 };
G(SEG_S) { 4,0, 0,0,  0,0, 0,3,  0,3, 4,3,  4,3, 4,6,  4,6, 0,6 };
G(SEG_T) { 0,0, 4,0,  2,0, 2,6 };
G(SEG_U) { 0,0, 0,6,  0,6, 4,6,  4,6, 4,0 };
G(SEG_V) { 0,0, 2,6,  2,6, 4,0 };
G(SEG_W) { 0,0, 0.8f,6,  0.8f,6, 2,2.8f,  2,2.8f, 3.2f,6,  3.2f,6, 4,0 };
G(SEG_X) { 0,0, 4,6,  4,0, 0,6 };
G(SEG_Y) { 0,0, 2,3,  4,0, 2,3,  2,3, 2,6 };
G(SEG_Z) { 0,0, 4,0,  4,0, 0,6,  0,6, 4,6 };

G(SEG_0) { 0,0, 4,0,  4,0, 4,6,  4,6, 0,6,  0,6, 0,0,  4,1.6f, 0,4.4f };
G(SEG_1) { 0.8f,1.2f, 2,0,  2,0, 2,6,  1,6, 3,6 };
G(SEG_2) { 0,0, 4,0,  4,0, 4,3,  4,3, 0,3,  0,3, 0,6,  0,6, 4,6 };
G(SEG_3) { 0,0, 4,0,  4,0, 4,6,  4,6, 0,6,  1.2f,3, 4,3 };
G(SEG_4) { 0,0, 0,3.4f,  0,3.4f, 4,3.4f,  4,0, 4,6 };
G(SEG_5) { 4,0, 0,0,  0,0, 0,3,  0,3, 4,3,  4,3, 4,6,  4,6, 0,6 };
G(SEG_6) { 4,0, 0,0,  0,0, 0,6,  0,6, 4,6,  4,6, 4,3,  4,3, 0,3 };
G(SEG_7) { 0,0, 4,0,  4,0, 1.6f,6 };
G(SEG_8) { 0,0, 4,0,  4,0, 4,6,  4,6, 0,6,  0,6, 0,0,  0,3, 4,3 };
G(SEG_9) { 4,6, 4,0,  4,0, 0,0,  0,0, 0,3,  0,3, 4,3 };

G(SEG_DASH)  { 0.6f,3, 3.4f,3 };
G(SEG_DOT)   { 1.7f,5.7f, 2.3f,5.7f };
G(SEG_COMMA) { 2.3f,5.0f, 1.4f,6.6f };
G(SEG_COLON) { 2,1.6f, 2,2.3f,  2,4.0f, 2,4.7f };
G(SEG_SLASH) { 4,0, 0,6 };
G(SEG_LPAR)  { 3,0, 1,1.6f,  1,1.6f, 1,4.4f,  1,4.4f, 3,6 };
G(SEG_RPAR)  { 1,0, 3,1.6f,  3,1.6f, 3,4.4f,  3,4.4f, 1,6 };
G(SEG_LT)    { 3.4f,0.6f, 0.6f,3,  0.6f,3, 3.4f,5.4f };
G(SEG_GT)    { 0.6f,0.6f, 3.4f,3,  3.4f,3, 0.6f,5.4f };
G(SEG_PLUS)  { 0.6f,3, 3.4f,3,  2,1.6f, 2,4.4f };
G(SEG_STAR)  { 2,1.2f, 2,4.8f,  0.6f,2, 3.4f,4,  3.4f,2, 0.6f,4 };

/* Percent, for the results screen. The two rings are strokes rather than
   little boxes: at five pixels wide a drawn ring closes up into a blob
   anyway, so a short stroke says the same thing with a third of the
   segments. */
G(SEG_PCT)   { 4,0.4f, 0,5.6f,  0.7f,0.8f, 0.7f,1.7f,  3.3f,4.3f, 3.3f,5.2f };

typedef struct {
    char         ch;
    int          segs;      /* segment count, four floats each */
    const float *data;
} Glyph;

#define GLYPH(c, arr) { c, (int)(sizeof(arr) / (4 * sizeof(float))), arr }

static const Glyph GLYPHS[] = {
    GLYPH('A', SEG_A), GLYPH('B', SEG_B), GLYPH('C', SEG_C), GLYPH('D', SEG_D),
    GLYPH('E', SEG_E), GLYPH('F', SEG_F), GLYPH('G', SEG_G), GLYPH('H', SEG_H),
    GLYPH('I', SEG_I), GLYPH('J', SEG_J), GLYPH('K', SEG_K), GLYPH('L', SEG_L),
    GLYPH('M', SEG_M), GLYPH('N', SEG_N), GLYPH('O', SEG_O), GLYPH('P', SEG_P),
    GLYPH('Q', SEG_Q), GLYPH('R', SEG_R), GLYPH('S', SEG_S), GLYPH('T', SEG_T),
    GLYPH('U', SEG_U), GLYPH('V', SEG_V), GLYPH('W', SEG_W), GLYPH('X', SEG_X),
    GLYPH('Y', SEG_Y), GLYPH('Z', SEG_Z),
    GLYPH('0', SEG_0), GLYPH('1', SEG_1), GLYPH('2', SEG_2), GLYPH('3', SEG_3),
    GLYPH('4', SEG_4), GLYPH('5', SEG_5), GLYPH('6', SEG_6), GLYPH('7', SEG_7),
    GLYPH('8', SEG_8), GLYPH('9', SEG_9),
    GLYPH('-', SEG_DASH),  GLYPH('.', SEG_DOT),   GLYPH(',', SEG_COMMA),
    GLYPH(':', SEG_COLON), GLYPH('/', SEG_SLASH), GLYPH('(', SEG_LPAR),
    GLYPH(')', SEG_RPAR),  GLYPH('<', SEG_LT),    GLYPH('>', SEG_GT),
    GLYPH('+', SEG_PLUS),  GLYPH('*', SEG_STAR),  GLYPH('%', SEG_PCT),
};

static const Glyph *s_index[128];
static bool s_indexed = false;

static void build_index(void)
{
    if (s_indexed) return;
    for (int i = 0; i < (int)ARRAY_COUNT(GLYPHS); ++i) {
        unsigned char c = (unsigned char)GLYPHS[i].ch;
        if (c < 128) s_index[c] = &GLYPHS[i];
    }
    s_indexed = true;
}

static char upcase(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/* Stroke half-width, in glyph units. Thick enough to read solid next to the
   filled artwork, thin enough that the counters in B, 8 and 0 stay open. */
#define HALF_W 0.42f

#define MAX_VERTS   1024
#define MAX_INDICES 1536

static SDL_Vertex s_verts[MAX_VERTS];
static int        s_idx[MAX_INDICES];

static void flush(Gfx *g, int *nv, int *ni)
{
    if (*ni > 0) SDL_RenderGeometry(g->renderer, NULL, s_verts, *nv, s_idx, *ni);
    *nv = 0;
    *ni = 0;
}

/* One segment as a quad. Ends are extended by the half-width so that strokes
   meeting at a corner close up instead of leaving a notch - cheaper and, at
   this size, indistinguishable from mitring the joint properly. */
static void stroke(Gfx *g, float x0, float y0, float x1, float y1, float hw,
                   SDL_Color c, int *nv, int *ni)
{
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
    dx /= len; dy /= len;

    float ex = dx * hw, ey = dy * hw;      /* end extension */
    float px = -dy * hw, py = dx * hw;     /* perpendicular */

    if (*nv + 4 > MAX_VERTS || *ni + 6 > MAX_INDICES) flush(g, nv, ni);

    float qx[4] = { x0 - ex + px, x1 + ex + px, x1 + ex - px, x0 - ex - px };
    float qy[4] = { y0 - ey + py, y1 + ey + py, y1 + ey - py, y0 - ey - py };

    int base = *nv;
    for (int i = 0; i < 4; ++i) {
        s_verts[base + i].position.x  = qx[i];
        s_verts[base + i].position.y  = qy[i];
        s_verts[base + i].color       = c;
        s_verts[base + i].tex_coord.x = 0.0f;
        s_verts[base + i].tex_coord.y = 0.0f;
    }
    *nv += 4;

    s_idx[(*ni)++] = base;     s_idx[(*ni)++] = base + 1; s_idx[(*ni)++] = base + 2;
    s_idx[(*ni)++] = base;     s_idx[(*ni)++] = base + 2; s_idx[(*ni)++] = base + 3;
}

void font_draw_scaled(Gfx *g, float x, float y, SDL_Color c, const char *text,
                      float scale)
{
    build_index();

    int nv = 0, ni = 0;
    float pen = x;
    float hw  = HALF_W * scale;

    for (const char *p = text; *p; ++p) {
        unsigned char ch = (unsigned char)upcase(*p);
        const Glyph *gl = (ch < 128) ? s_index[ch] : NULL;
        if (gl) {
            for (int s = 0; s < gl->segs; ++s) {
                const float *v = &gl->data[s * 4];
                stroke(g, pen + v[0] * scale, y + v[1] * scale,
                          pen + v[2] * scale, y + v[3] * scale,
                       hw, c, &nv, &ni);
            }
        }
        pen += FONT_ADVANCE * scale;
    }

    flush(g, &nv, &ni);
}

void font_draw(Gfx *g, int x, int y, SDL_Color c, const char *text)
{
    font_draw_scaled(g, (float)x, (float)y, c, text, 1.0f);
}

int font_width(const char *text)
{
    int len = (int)strlen(text);
    return len > 0 ? len * FONT_ADVANCE - 1 : 0;
}

float font_width_scaled(const char *text, float scale)
{
    int len = (int)strlen(text);
    return len > 0 ? (len * FONT_ADVANCE - 1) * scale : 0.0f;
}
