#include "tuning.h"
#include "common.h"
#include "formation.h"
#include "game.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TUNE_SECTION(title)
#define TUNE_I(name, def, lo, hi, doc) def,
#define TUNE_F(name, def, lo, hi, doc) def,
static const Tuning DEFAULTS = {
#include "tuning.def"
};
Tuning tune = {
#include "tuning.def"
};
#undef TUNE_I
#undef TUNE_F
#undef TUNE_SECTION

typedef struct {
    const char *name;      /* NULL for a section heading */
    const char *text;      /* the doc line, or the heading */
    size_t      off;
    bool        is_float;
    double      lo, hi;
} Knob;

static const Knob KNOBS[] = {
#define TUNE_SECTION(title) { NULL, title, 0, false, 0.0, 0.0 },
#define TUNE_I(name, def, lo, hi, doc) \
    { #name, doc, offsetof(Tuning, name), false, (double)(lo), (double)(hi) },
#define TUNE_F(name, def, lo, hi, doc) \
    { #name, doc, offsetof(Tuning, name), true,  (double)(lo), (double)(hi) },
#include "tuning.def"
#undef TUNE_I
#undef TUNE_F
#undef TUNE_SECTION
};

static double knob_get(const Tuning *t, const Knob *k)
{
    const char *p = (const char *)t + k->off;
    return k->is_float ? (double)*(const float *)p : (double)*(const int *)p;
}

static void knob_set(Tuning *t, const Knob *k, double v)
{
    char *p = (char *)t + k->off;
    if (k->is_float) *(float *)p = (float)v;
    else             *(int *)p   = (int)v;
}

static void knob_format(char *out, size_t n, const Knob *k, double v)
{
    if (k->is_float) snprintf(out, n, "%g", v);
    else             snprintf(out, n, "%.0f", v);
}

static bool name_is(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return false;
    }
    return *a == *b;
}

static const Knob *knob_find(const char *name)
{
    for (int i = 0; i < ARRAY_COUNT(KNOBS); ++i) {
        if (KNOBS[i].name && name_is(KNOBS[i].name, name)) return &KNOBS[i];
    }
    return NULL;
}

static void upcase(char *s)
{
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

/* ------------------------------------------------------------ parsing */

/* Rules between two values. Formation's own live beside the code they protect;
   this one has nowhere better to be. */
static const char *check_rules(const Tuning *t)
{
    if (t->MENU_RELEASE >= t->MENU_PRESS) return "MENU_RELEASE MUST BE UNDER MENU_PRESS";
    return wave_tuning_check(t);
}

/* Reads `path` over the defaults into `out`. On failure `err` holds the first
   problem, upper case, short enough for the screen. */
static bool parse_file(const char *path, Tuning *out, char *err, size_t errn)
{
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(err, errn, "CANNOT OPEN %s", path); upcase(err); return false; }

    *out = DEFAULTS;
    bool set[ARRAY_COUNT(KNOBS)] = { false };
    char line[256];
    int  lineno = 0;
    bool ok     = true;

    while (ok && fgets(line, sizeof line, f)) {
        ++lineno;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;

        char *p = line;
        while (isspace((unsigned char)*p)) ++p;
        if (!*p) continue;

        char name[64];
        int  len = 0;
        while ((isalnum((unsigned char)*p) || *p == '_') && len < (int)sizeof name - 1) {
            name[len++] = *p++;
        }
        name[len] = 0;
        while (isspace((unsigned char)*p)) ++p;
        if (*p == '=') ++p;

        const Knob *k = knob_find(name);
        if (!len || !k) {
            snprintf(err, errn, "LINE %d: UNKNOWN %.24s", lineno, len ? name : p);
            ok = false;
            break;
        }

        char  *end;
        double v = strtod(p, &end);
        if (end != p && k->is_float && (*end == 'f' || *end == 'F')) ++end;
        while (isspace((unsigned char)*end)) ++end;
        if (end == p || *end) {
            snprintf(err, errn, "LINE %d: %s IS NOT A NUMBER", lineno, k->name);
            ok = false;
        } else if (!k->is_float && v != (double)(long long)v) {
            snprintf(err, errn, "LINE %d: %s WANTS A WHOLE NUMBER", lineno, k->name);
            ok = false;
        } else if (v < k->lo || v > k->hi) {
            char lo[24], hi[24];
            knob_format(lo, sizeof lo, k, k->lo);
            knob_format(hi, sizeof hi, k, k->hi);
            snprintf(err, errn, "LINE %d: %s OUT OF %s-%s", lineno, k->name, lo, hi);
            ok = false;
        } else if (set[k - KNOBS]) {
            snprintf(err, errn, "LINE %d: %s SET TWICE", lineno, k->name);
            ok = false;
        } else {
            set[k - KNOBS] = true;
            knob_set(out, k, v);
        }
    }
    fclose(f);

    if (ok) {
        const char *rule = check_rules(out);
        if (rule) { snprintf(err, errn, "%s", rule); ok = false; }
    }
    upcase(err);
    return ok;
}

/* ------------------------------------------------------------- the file */

typedef struct { bool exists; long long mtime; long long size; } Stamp;

static char  s_path[512];
static Stamp s_loaded;     /* the file as it was when last read */
static Stamp s_seen;       /* and as the previous poll found it */
static char  s_status[160];

static Stamp stamp_of(const char *path)
{
    Stamp s = { false, 0, 0 };
#ifdef _WIN32
    struct _stat64 st;
    if (_stat64(path, &st) == 0) {
#else
    struct stat st;
    if (stat(path, &st) == 0) {
#endif
        s.exists = true;
        s.mtime  = (long long)st.st_mtime;
        s.size   = (long long)st.st_size;
    }
    return s;
}

static bool stamp_same(Stamp a, Stamp b)
{
    return a.exists == b.exists && a.mtime == b.mtime && a.size == b.size;
}

/* Puts `next` in force and says what moved, on stdout per value and on the
   status line in summary. */
static void apply(const Tuning *next, bool first)
{
    int         changed = 0;
    const Knob *only    = NULL;
    char        was[24] = "", now[24] = "";

    for (int i = 0; i < ARRAY_COUNT(KNOBS); ++i) {
        const Knob *k = &KNOBS[i];
        if (!k->name) continue;
        double a = knob_get(&tune, k), b = knob_get(next, k);
        if (a == b) continue;
        knob_format(was, sizeof was, k, a);
        knob_format(now, sizeof now, k, b);
        printf("tuning: %s %s -> %s\n", k->name, was, now);
        only = k;
        ++changed;
    }
    tune = *next;

    int over = tuning_overrides();
    if (first) {
        snprintf(s_status, sizeof s_status, "TUNING: %d VALUE%s FROM FILE",
                 over, over == 1 ? "" : "S");
    } else if (changed == 1) {
        snprintf(s_status, sizeof s_status, "TUNED %s\n%s > %s", only->name, was, now);
    } else {
        snprintf(s_status, sizeof s_status, "TUNED %d VALUES\n%d DIFFER%s FROM DEFAULT",
                 changed, over, over == 1 ? "S" : "");
    }
    fflush(stdout);
}

static int reload(bool first)
{
    s_loaded = s_seen = stamp_of(s_path);

    if (!s_loaded.exists) {
        apply(&DEFAULTS, false);
        snprintf(s_status, sizeof s_status, "TUNING FILE GONE\nBACK TO DEFAULTS");
        return 1;
    }

    Tuning next;
    char   err[96] = "";
    if (!parse_file(s_path, &next, err, sizeof err)) {
        snprintf(s_status, sizeof s_status, "TUNING REJECTED\n%s", err);
        printf("tuning: %s rejected - %s\n", s_path, err);
        fflush(stdout);
        return -1;
    }
    apply(&next, first);
    return 1;
}

bool tuning_open(const char *path)
{
    bool explicit_path = path != NULL;
    snprintf(s_path, sizeof s_path, "%s", path ? path : "tuning.cfg");
    s_loaded = s_seen = stamp_of(s_path);

    if (!s_loaded.exists) {
        if (!explicit_path) return true;      /* the shipped game */
        printf("tuning: %s not found\n", s_path);
        snprintf(s_status, sizeof s_status, "TUNING FILE NOT FOUND");
        return false;
    }
    if (reload(true) < 0) return false;
    printf("tuning: %d values from %s\n", tuning_overrides(), s_path);
    fflush(stdout);
    return true;
}

int tuning_poll(void)
{
    if (!s_path[0]) return 0;
    Stamp now = stamp_of(s_path);
    if (stamp_same(now, s_loaded)) { s_seen = now; return 0; }
    if (!stamp_same(now, s_seen))  { s_seen = now; return 0; }
    return reload(false);
}

const char *tuning_status(void) { return s_status; }
const char *tuning_path(void)   { return s_path; }

int tuning_overrides(void)
{
    int n = 0;
    for (int i = 0; i < ARRAY_COUNT(KNOBS); ++i) {
        if (KNOBS[i].name && knob_get(&tune, &KNOBS[i]) != knob_get(&DEFAULTS, &KNOBS[i])) ++n;
    }
    return n;
}

bool tuning_write_template(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { printf("tuning: cannot write %s\n", path); return false; }

    fprintf(f,
        "# Claudaga tuning. Uncomment a line and change its value to override it;\n"
        "# anything left commented keeps the built-in default. The running game\n"
        "# rereads this file when it changes. A file with any error is rejected\n"
        "# whole and the game keeps the values it had.\n");

    for (int i = 0; i < ARRAY_COUNT(KNOBS); ++i) {
        const Knob *k = &KNOBS[i];
        if (!k->name) { fprintf(f, "\n# --- %s ---\n", k->text); continue; }
        char def[24], lo[24], hi[24], assign[64];
        knob_format(def, sizeof def, k, knob_get(&DEFAULTS, k));
        knob_format(lo,  sizeof lo,  k, k->lo);
        knob_format(hi,  sizeof hi,  k, k->hi);
        snprintf(assign, sizeof assign, "%s = %s", k->name, def);
        fprintf(f, "# %-28s # %s [%s-%s]\n", assign, k->text, lo, hi);
    }
    fclose(f);
    printf("wrote %s\n", path);
    return true;
}
