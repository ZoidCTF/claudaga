#include "shape.h"

#include <math.h>

/* One shape is a few polygons of a few vertices each, doubled where a piece is
   mirrored. These are sized well above the largest drawing rather than
   computed, because the whole point is that a draw call touches no allocator. */
#define MAX_BATCH_VERTS   256
#define MAX_BATCH_INDICES 768

static SDL_Vertex s_verts[MAX_BATCH_VERTS];
static int        s_indices[MAX_BATCH_INDICES];

/* Appends one convex polygon to the batch as a triangle fan. Returns false if
   it would not fit, which for artwork this size means the limits above are
   wrong rather than that the caller should cope. */
static bool push_fan(const Vec2 *pts, int n, SDL_Color c, int *nv, int *ni)
{
    if (n < 3) return true;
    if (*nv + n > MAX_BATCH_VERTS) return false;
    if (*ni + (n - 2) * 3 > MAX_BATCH_INDICES) return false;

    int base = *nv;
    for (int i = 0; i < n; ++i) {
        s_verts[base + i].position.x = pts[i].x;
        s_verts[base + i].position.y = pts[i].y;
        s_verts[base + i].color      = c;
        s_verts[base + i].tex_coord.x = 0.0f;
        s_verts[base + i].tex_coord.y = 0.0f;
    }
    *nv += n;

    for (int i = 1; i + 1 < n; ++i) {
        s_indices[(*ni)++] = base;
        s_indices[(*ni)++] = base + i;
        s_indices[(*ni)++] = base + i + 1;
    }
    return true;
}

static void flush(Gfx *g, int nv, int ni)
{
    if (ni > 0) {
        /* No texture, so the vertex colours are the fill. */
        SDL_RenderGeometry(g->renderer, NULL, s_verts, nv, s_indices, ni);
    }
}

void shape_draw_poly(Gfx *g, const Vec2 *pts, int n, SDL_Color color)
{
    int nv = 0, ni = 0;
    if (!push_fan(pts, n, color, &nv, &ni)) return;
    flush(g, nv, ni);
}

void shape_draw(Gfx *g, ShapeId id, Vec2 pos, float heading, float scale)
{
    shape_draw_pal(g, id, pos, heading, scale, NULL, 1.0f);
}

void shape_draw_pal(Gfx *g, ShapeId id, Vec2 pos, float heading, float scale,
                    const ShapePalette *pal, float alpha)
{
    const Shape *s = shape_get(id);
    if (!s) return;
    if (!pal) pal = s->palette;

    /* Screen y grows downward, so this matrix turns clockwise on screen, which
       matches the heading convention the rest of the game uses: 0 is north and
       the angle increases towards east. */
    float rad = heading * (float)M_PI / 180.0f;
    float ca  = cosf(rad) * scale;
    float sa  = sinf(rad) * scale;

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    int nv = 0, ni = 0;
    Vec2 work[16];

    for (int p = 0; p < s->poly_count; ++p) {
        const ShapePoly *poly = &s->poly[p];
        int n = poly->count;
        if (n > (int)ARRAY_COUNT(work)) n = (int)ARRAY_COUNT(work);

        SDL_Color c = pal->c[poly->pal < SHAPE_PAL_MAX ? poly->pal : 0];
        c.a = (Uint8)(c.a * alpha);

        for (int pass = 0; pass < (poly->mirror ? 2 : 1); ++pass) {
            float mx = pass ? -1.0f : 1.0f;
            for (int i = 0; i < n; ++i) {
                Vec2 v = s->vert[poly->first + i];
                float lx = v.x * mx;
                float ly = v.y;
                work[i].x = pos.x + lx * ca - ly * sa;
                work[i].y = pos.y + lx * sa + ly * ca;
            }
            if (!push_fan(work, n, c, &nv, &ni)) {
                flush(g, nv, ni);
                nv = ni = 0;
                push_fan(work, n, c, &nv, &ni);
            }
        }
    }

    flush(g, nv, ni);
}
