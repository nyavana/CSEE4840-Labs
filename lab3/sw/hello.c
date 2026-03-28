/*
 * Userspace program that communicates with the vga_ball device driver
 * through ioctls
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
#include <string.h>
#include <unistd.h>

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#define BALL_RADIUS   20

int vga_ball_fd;

/* Set the background color */
void set_background_color(const vga_ball_color_t *c)
{
  vga_ball_arg_t vla;
  vla.background = *c;
  if (ioctl(vga_ball_fd, VGA_BALL_WRITE_BACKGROUND, &vla)) {
      perror("ioctl(VGA_BALL_WRITE_BACKGROUND) failed");
      return;
  }
}

/* Set the ball position */
void set_ball_position(int x, int y)
{
  vga_ball_arg_t vla;
  vla.position.x = x;
  vla.position.y = y;
  if (ioctl(vga_ball_fd, VGA_BALL_WRITE_POSITION, &vla)) {
      perror("ioctl(VGA_BALL_WRITE_POSITION) failed");
      return;
  }
}

int main()
{
  static const char filename[] = "/dev/vga_ball";
  int ball_x = SCREEN_WIDTH / 2, ball_y = SCREEN_HEIGHT / 2;
  int vel_x = 3, vel_y = 2;

  printf("VGA ball Userspace program started\n");

  if ( (vga_ball_fd = open(filename, O_RDWR)) == -1) {
    fprintf(stderr, "could not open %s\n", filename);
    return -1;
  }

  /* Set dark blue background */
  {
    vga_ball_color_t bg = { 0x00, 0x00, 0x80 };
    set_background_color(&bg);
  }

  printf("Bouncing ball started. Press Ctrl+C to stop.\n");

  for (;;) {
    ball_x += vel_x;
    ball_y += vel_y;

    /* Bounce off left/right walls */
    if (ball_x - BALL_RADIUS < 0) {
      ball_x = BALL_RADIUS;
      vel_x = -vel_x;
    } else if (ball_x + BALL_RADIUS >= SCREEN_WIDTH) {
      ball_x = SCREEN_WIDTH - 1 - BALL_RADIUS;
      vel_x = -vel_x;
    }

    /* Bounce off top/bottom walls */
    if (ball_y - BALL_RADIUS < 0) {
      ball_y = BALL_RADIUS;
      vel_y = -vel_y;
    } else if (ball_y + BALL_RADIUS >= SCREEN_HEIGHT) {
      ball_y = SCREEN_HEIGHT - 1 - BALL_RADIUS;
      vel_y = -vel_y;
    }

    set_ball_position(ball_x, ball_y);
    usleep(16667); /* ~60 fps */
  }

  printf("VGA BALL Userspace program terminating\n");
  return 0;
}
