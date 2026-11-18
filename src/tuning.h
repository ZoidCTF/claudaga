#ifndef CLAUDAGA_TUNING_H
#define CLAUDAGA_TUNING_H

#include <stdbool.h>

/* The tunables in tuning.def, read at run time from a text file of
 * `NAME = value` lines (# starts a comment). A name not in the file keeps its
 * default, so a missing file is the shipped game and deleting a line undoes
 * it. A file with any error - an unknown name, a bad number, a value out of
 * range, a broken rule between two values - is rejected whole and the values
 * in force stay as they were: a half-typed edit never runs. */

typedef struct {
#define TUNE_I(name, def, lo, hi, doc) int   name;
#define TUNE_F(name, def, lo, hi, doc) float name;
#define TUNE_SECTION(title)
#include "tuning.def"
#undef TUNE_I
#undef TUNE_F
#undef TUNE_SECTION
} Tuning;

extern Tuning tune;

/* Where values come from. NULL means tuning.cfg in the working directory, or
   else beside the exe; while neither exists both are watched. Loads it once
   and reports on stdout. Returns false when the
   file was rejected, or was named explicitly and is not there; either way the
   defaults are in force and the file is still watched. */
bool tuning_open(const char *path);

/* Checks whether the file has changed since it was read and, once it has held
   still for one call, reads it again - an editor saving in pieces would
   otherwise be read half written. Call it a few times a second. Returns 1 when
   new values took effect, -1 when the file was rejected, 0 otherwise. */
int  tuning_poll(void);

/* The last load's outcome in a line or two of upper case, for the screen, and
   how many values differ from the defaults. */
const char *tuning_status(void);
int  tuning_overrides(void);
const char *tuning_path(void);

/* Writes every tunable with its default and range, all commented out. */
bool tuning_write_template(const char *path);

#endif /* CLAUDAGA_TUNING_H */
