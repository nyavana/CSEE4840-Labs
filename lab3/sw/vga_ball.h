#ifndef _VGA_BALL_H
#define _VGA_BALL_H
#include <linux/ioctl.h>

typedef struct {
  unsigned short x, y;
} vga_ball_coord_t;

typedef struct {
  vga_ball_coord_t position;
} vga_ball_arg_t;

#define VGA_BALL_MAGIC 'q'
#define VGA_BALL_SET_POSITION _IOW(VGA_BALL_MAGIC, 1, vga_ball_arg_t)
#define VGA_BALL_GET_POSITION _IOR(VGA_BALL_MAGIC, 2, vga_ball_arg_t)

#endif
