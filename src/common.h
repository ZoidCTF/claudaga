#ifndef CLAUDAGA_COMMON_H
#define CLAUDAGA_COMMON_H

#include <stdint.h>
#include <stdbool.h>

/* The arcade board runs a 288x224 raster rotated 90 degrees for a vertical
   cabinet, so the picture the player sees is 224 wide by 288 tall. Everything
   in the game works in these units; the window scales them up at the end. */
#define GAME_W 224
#define GAME_H 288

/* Nearly every actor in Galaga is a 16x16 cell. Explosions are the exception
   and use a 32x32 cell. */
#define CELL   16

/* The row the fighter flies along. Enemies aim their missiles at it, so it is
   shared rather than living in whichever module happens to draw the ship. */
#define PLAYER_Y (GAME_H - 24)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  i32;

typedef struct { float x, y; } Vec2;

/* Headings are degrees clockwise from straight up, matching the way the
   sprite sheet is laid out: 0 is north, 90 east, 180 south, 270 west. */
#define HEADING_N 0.0f
#define HEADING_E 90.0f
#define HEADING_S 180.0f
#define HEADING_W 270.0f

#define ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

#endif /* CLAUDAGA_COMMON_H */
