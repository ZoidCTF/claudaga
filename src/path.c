#include "path.h"

#include <math.h>
#include <SDL.h>

/* Standard uniform Catmull-Rom: the curve runs from p1 to p2, with p0 and p3
   only setting the tangents at those ends. */
static Vec2 catmull_rom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    Vec2 r;
    r.x = 0.5f * ((2.0f * p1.x) +
                  (-p0.x + p2.x) * t +
                  (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                  (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
    r.y = 0.5f * ((2.0f * p1.y) +
                  (-p0.y + p2.y) * t +
                  (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                  (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
    return r;
}

void path_build(Path *p, const Vec2 *ctrl, int nctrl)
{
    p->n = 0;
    p->length = 0.0f;
    if (nctrl < 2) return;

    /* Spread the sample budget over the segments. Sampling by parameter makes
       the spacing uneven, which is fine - path_point interpolates by distance,
       so tight corners simply end up better represented. */
    int segs = nctrl - 1;
    int per  = (PATH_MAX_SAMPLES - 1) / segs;
    if (per < 2) per = 2;

    for (int i = 0; i < segs; ++i) {
        /* Clamp at the ends so the curve starts and stops where it is told. */
        Vec2 p0 = ctrl[i > 0 ? i - 1 : 0];
        Vec2 p1 = ctrl[i];
        Vec2 p2 = ctrl[i + 1];
        Vec2 p3 = ctrl[i + 2 < nctrl ? i + 2 : nctrl - 1];

        /* Every segment emits its start; the final endpoint is added after. */
        for (int k = 0; k < per; ++k) {
            if (p->n >= PATH_MAX_SAMPLES) break;
            p->pt[p->n++] = catmull_rom(p0, p1, p2, p3, (float)k / (float)per);
        }
    }
    if (p->n < PATH_MAX_SAMPLES) p->pt[p->n++] = ctrl[nctrl - 1];

    p->dist[0] = 0.0f;
    for (int i = 1; i < p->n; ++i) {
        float dx = p->pt[i].x - p->pt[i - 1].x;
        float dy = p->pt[i].y - p->pt[i - 1].y;
        p->dist[i] = p->dist[i - 1] + sqrtf(dx * dx + dy * dy);
    }
    p->length = p->dist[p->n - 1];
}

/* Index of the last sample at or before `s`, by binary search. */
static int sample_before(const Path *p, float s)
{
    int lo = 0, hi = p->n - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (p->dist[mid] <= s) lo = mid;
        else                   hi = mid - 1;
    }
    return lo;
}

Vec2 path_point(const Path *p, float s)
{
    if (p->n == 0) { Vec2 z = { 0.0f, 0.0f }; return z; }
    if (s <= 0.0f)        return p->pt[0];
    if (s >= p->length)   return p->pt[p->n - 1];

    int i = sample_before(p, s);
    if (i >= p->n - 1) return p->pt[p->n - 1];

    float span = p->dist[i + 1] - p->dist[i];
    float t    = span > 0.0f ? (s - p->dist[i]) / span : 0.0f;

    Vec2 r;
    r.x = p->pt[i].x + (p->pt[i + 1].x - p->pt[i].x) * t;
    r.y = p->pt[i].y + (p->pt[i + 1].y - p->pt[i].y) * t;
    return r;
}

float path_heading(const Path *p, float s)
{
    if (p->n < 2) return HEADING_S;

    /* Measure across a short span rather than between neighbouring samples:
       adjacent points can be a fraction of a pixel apart, and the rounding in
       that difference makes the heading jitter. */
    const float LOOK = 4.0f;
    float a = s - LOOK * 0.5f;
    float b = s + LOOK * 0.5f;
    if (a < 0.0f)             { a = 0.0f; b = LOOK; }
    if (b > p->length)        { b = p->length; a = b - LOOK; }
    if (a < 0.0f)             a = 0.0f;

    Vec2 pa = path_point(p, a);
    Vec2 pb = path_point(p, b);
    float dx = pb.x - pa.x;
    float dy = pb.y - pa.y;
    if (dx == 0.0f && dy == 0.0f) return HEADING_S;

    return heading_from_vec(dx, dy);
}

float heading_from_vec(float dx, float dy)
{
    if (dx == 0.0f && dy == 0.0f) return HEADING_S;
    /* Screen y grows downward, so north is -y. atan2(dx, -dy) gives degrees
       clockwise from north, which is the convention the sprite sheet uses. */
    float deg = (float)(atan2((double)dx, (double)-dy) * 180.0 / M_PI);
    if (deg < 0.0f) deg += 360.0f;
    return deg;
}

/* ---------------------------------------------------------------- hermite */

Vec2 hermite_point(Vec2 p0, Vec2 t0, Vec2 p1, Vec2 t1, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    float h00 =  2.0f * t3 - 3.0f * t2 + 1.0f;
    float h10 =         t3 - 2.0f * t2 + t;
    float h01 = -2.0f * t3 + 3.0f * t2;
    float h11 =         t3 -        t2;

    Vec2 r;
    r.x = h00 * p0.x + h10 * t0.x + h01 * p1.x + h11 * t1.x;
    r.y = h00 * p0.y + h10 * t0.y + h01 * p1.y + h11 * t1.y;
    return r;
}

Vec2 hermite_tangent(Vec2 p0, Vec2 t0, Vec2 p1, Vec2 t1, float t)
{
    float t2 = t * t;
    float d00 =  6.0f * t2 - 6.0f * t;
    float d10 =  3.0f * t2 - 4.0f * t + 1.0f;
    float d01 = -6.0f * t2 + 6.0f * t;
    float d11 =  3.0f * t2 - 2.0f * t;

    Vec2 r;
    r.x = d00 * p0.x + d10 * t0.x + d01 * p1.x + d11 * t1.x;
    r.y = d00 * p0.y + d10 * t0.y + d01 * p1.y + d11 * t1.y;
    return r;
}

float hermite_length(Vec2 p0, Vec2 t0, Vec2 p1, Vec2 t1)
{
    /* Chord sum over a modest number of steps. The join is a short, gentle
       curve, so this is accurate enough to time the approach by. */
    const int STEPS = 16;
    float len = 0.0f;
    Vec2 prev = p0;
    for (int i = 1; i <= STEPS; ++i) {
        Vec2 cur = hermite_point(p0, t0, p1, t1, (float)i / (float)STEPS);
        float dx = cur.x - prev.x, dy = cur.y - prev.y;
        len += sqrtf(dx * dx + dy * dy);
        prev = cur;
    }
    return len;
}

void path_mirror(Path *dst, const Path *src)
{
    dst->n      = src->n;
    dst->length = src->length;
    for (int i = 0; i < src->n; ++i) {
        dst->pt[i].x = (float)GAME_W - src->pt[i].x;
        dst->pt[i].y = src->pt[i].y;
        dst->dist[i] = src->dist[i];
    }
}

float heading_delta(float from, float to)
{
    float d = fmodf(to - from, 360.0f);
    if (d < -180.0f) d += 360.0f;
    if (d >  180.0f) d -= 360.0f;
    return d;
}
