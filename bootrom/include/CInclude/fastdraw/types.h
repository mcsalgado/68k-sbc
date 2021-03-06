#ifndef FASTDRAW_TYPES_H
#define FASTDRAW_TYPES_H

#include <stdint.h>

typedef struct point_t
{
  uint16_t x, y;
} Point;

typedef struct pen_t
{
  uint16_t position_x;
  uint16_t position_y;
  uint16_t size_x;
  uint16_t size_y;
  uint8_t color;
} Pen;

typedef struct rectangle_t
{
  uint16_t x, y, width, height;
} Rectangle;

typedef enum {
  DRAW_ModeReplace = 0x00,
  DRAW_ModeAND = 0x08,
  DRAW_ModeOR = 0x10,
  DRAW_ModeXOR = 0x18
} DRAW_LogicalMode;

#endif
