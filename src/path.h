#ifndef CLAUDAGA_PATH_H
#define CLAUDAGA_PATH_H

#include "common.h"

/* An entry path, baked from a handful of control points into a dense polyline
 * with running arc length. Two things matter for the entry animation and both
 * fall out of that: an enemy must travel at a steady speed no matter how the
 * curve bunches up, and it must point along its direction of travel. Walking a
 * spline by its raw parameter gives neither, so the curve is resampled by
 * distance once at startup and everything after that is a lookup. */

#define PATH_MAX_SAMPLES 512

typedef struct {
    Vec2  pt[PATH_MAX_SAMPLES];
    float dist[PATH_MAX_SAMPLES];   /* arc length from the start to pt[i] */
    int   n;
    float length;
} Path;

/* Bakes a Catmull-Rom spline through `ctrl`. The curve passes through every
   control point, which is what makes these paths practical to author by hand.
   The first and last points are duplicated to anchor the end tangents. */
void path_build(Path *p, const Vec2 *ctrl, int nctrl);

/* Position at `s` pixels along the path. Clamps at both ends. */
Vec2 path_point(const Path *p, float s);

/* Direction of travel at `s`, in degrees clockwise from north. */
float path_heading(const Path *p, float s);

/* Mirrors a path across the screen's vertical centre line, so the left and
   right halves of a symmetric entry only need authoring once. */
void path_mirror(Path *dst, const Path *src);

/* A cubic Hermite curve, used to join the end of a shared entry path to each
 * enemy's own formation slot. Hermite is the right tool here because it is
 * defined by its endpoint tangents: feeding in the path's exit direction makes
 * the join continue the flight rather than kink away from it, and pinning the
 * arrival tangent means the enemy is already facing the way it will sit. */
Vec2  hermite_point(Vec2 p0, Vec2 t0, Vec2 p1, Vec2 t1, float t);
Vec2  hermite_tangent(Vec2 p0, Vec2 t0, Vec2 p1, Vec2 t1, float t);
float hermite_length(Vec2 p0, Vec2 t0, Vec2 p1, Vec2 t1);

/* Direction of a vector in degrees clockwise from north. */
float heading_from_vec(float dx, float dy);

/* Shortest signed turn from `from` to `to`, in degrees, in -180..180. Used to
   ease a heading round without spinning the long way. */
float heading_delta(float from, float to);

#endif /* CLAUDAGA_PATH_H */
