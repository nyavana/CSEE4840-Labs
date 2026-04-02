/*
 * Userspace program that bounces a ball on the VGA display
 * by communicating coordinates to the vga_ball device driver
 *
 * Stephen A. Edwards
 * Columbia University
 */
#include <stdio.h>
#include "vga_ball.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SCREEN_W 640
#define SCREEN_H 480
#define RADIUS   16
#define X_MIN RADIUS
#define X_MAX (SCREEN_W - 1 - RADIUS)
#define Y_MIN RADIUS
#define Y_MAX (SCREEN_H - 1 - RADIUS)

int vga_ball_fd;

void set_ball_position(int x, int y)
{
  vga_ball_arg_t vla;
  vla.position.x = x;
  vla.position.y = y;
  if (ioctl(vga_ball_fd, VGA_BALL_SET_POSITION, &vla)) {
      perror("ioctl(VGA_BALL_SET_POSITION) failed");
  }
}

int main()
{
  static const char filename[] = "/dev/vga_ball";
  int x = SCREEN_W / 2, y = SCREEN_H / 2;
  int dx = 3, dy = 2;

  printf("VGA ball Userspace program started\n");

  if ((vga_ball_fd = open(filename, O_RDWR)) == -1) {
    fprintf(stderr, "could not open %s\n", filename);
    return -1;
  }

  for (;;) {
    x += dx;
    y += dy;
    if (x <= X_MIN || x >= X_MAX) {
      dx = -dx;
      x += dx;
    }
    if (y <= Y_MIN || y >= Y_MAX) {
      dy = -dy;
      y += dy;
    }
    set_ball_position(x, y);
    usleep(16667); /* ~60 fps */
  }

  return 0;
}
