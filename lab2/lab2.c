/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Please Changeto Yourname (pcy2301)
 */
#include "fbputchar.h"
#include "usbkeyboard.h"

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128
#define SCREEN_ROWS 24
#define SCREEN_COLS 64
#define INPUT_ROWS 2

#define INPUT_TOP_ROW (SCREEN_ROWS - INPUT_ROWS)
#define DIVIDER_ROW (INPUT_TOP_ROW - 1)
#define RECV_TOP_ROW 0
#define RECV_BOTTOM_ROW (DIVIDER_ROW - 1)
#define RECV_ROWS (RECV_BOTTOM_ROW - RECV_TOP_ROW + 1)

#define INPUT_MAX_CHARS (INPUT_ROWS * SCREEN_COLS)

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

int sockfd = -1; /* Socket file descriptor */

struct libusb_device_handle *keyboard = NULL;
uint8_t endpoint_address;

pthread_t network_thread;
pthread_mutex_t fb_lock = PTHREAD_MUTEX_INITIALIZER;

static char recv_lines[RECV_ROWS][SCREEN_COLS];
static int recv_row = 0;
static int recv_col = 0;

static char input_buf[INPUT_MAX_CHARS + 1];
static size_t input_len = 0;
static size_t cursor_pos = 0;

void *network_thread_f(void *);

static void clear_row_locked(int row)
{
  int col;

  for (col = 0; col < SCREEN_COLS; col++) {
    fbputchar(' ', row, col);
  }
}

static void clear_screen_locked(void)
{
  int row;

  for (row = 0; row < SCREEN_ROWS; row++) {
    clear_row_locked(row);
  }
}

static void draw_divider_locked(void)
{
  int col;

  for (col = 0; col < SCREEN_COLS; col++) {
    fbputchar('-', DIVIDER_ROW, col);
  }
}

static void render_receive_row_locked(int logical_row)
{
  int col;

  for (col = 0; col < SCREEN_COLS; col++) {
    fbputchar(recv_lines[logical_row][col], RECV_TOP_ROW + logical_row, col);
  }
}

static void clear_receive_row_locked(int logical_row)
{
  memset(recv_lines[logical_row], ' ', SCREEN_COLS);
  render_receive_row_locked(logical_row);
}

static void redraw_receive_region_locked(void)
{
  int row;

  for (row = 0; row < RECV_ROWS; row++) {
    render_receive_row_locked(row);
  }
}

static void scroll_receive_locked(void)
{
  int row;

  for (row = 1; row < RECV_ROWS; row++) {
    memcpy(recv_lines[row - 1], recv_lines[row], SCREEN_COLS);
  }
  memset(recv_lines[RECV_ROWS - 1], ' ', SCREEN_COLS);
  redraw_receive_region_locked();
}

static void advance_receive_line_locked(void)
{
  recv_col = 0;
  recv_row++;
  if (recv_row >= RECV_ROWS) {
    scroll_receive_locked();
    recv_row = RECV_ROWS - 1;
  }
  clear_receive_row_locked(recv_row);
}

static void append_receive_text_locked(const char *text, size_t len)
{
  size_t i;
  char c;

  for (i = 0; i < len; i++) {
    c = text[i];

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      advance_receive_line_locked();
      continue;
    }

    if (c < 32 || c > 126) {
      c = '?';
    }

    if (recv_col >= SCREEN_COLS) {
      advance_receive_line_locked();
    }

    recv_lines[recv_row][recv_col] = c;
    fbputchar(c, RECV_TOP_ROW + recv_row, recv_col);
    recv_col++;

    if (recv_col >= SCREEN_COLS) {
      advance_receive_line_locked();
    }
  }
}

static void clear_input_rows_locked(void)
{
  int row;

  for (row = INPUT_TOP_ROW; row < SCREEN_ROWS; row++) {
    clear_row_locked(row);
  }
}

static void draw_cursor_locked(void)
{
  size_t visible_pos = cursor_pos;
  int cursor_col;

  if (visible_pos >= INPUT_MAX_CHARS && INPUT_MAX_CHARS > 0) {
    visible_pos = INPUT_MAX_CHARS - 1;
  }
  cursor_col = (int)(visible_pos % SCREEN_COLS);

  draw_divider_locked();
  fbputchar('^', DIVIDER_ROW, cursor_col);
}

static void render_input_locked(void)
{
  size_t i;
  int row;
  int col;

  clear_input_rows_locked();
  for (i = 0; i < input_len; i++) {
    row = INPUT_TOP_ROW + (int)(i / SCREEN_COLS);
    col = (int)(i % SCREEN_COLS);
    fbputchar(input_buf[i], row, col);
  }
  draw_cursor_locked();
}

static void initialize_ui(void)
{
  memset(recv_lines, ' ', sizeof(recv_lines));
  recv_row = 0;
  recv_col = 0;

  input_len = 0;
  cursor_pos = 0;
  input_buf[0] = '\0';

  pthread_mutex_lock(&fb_lock);
  clear_screen_locked();
  draw_divider_locked();
  redraw_receive_region_locked();
  render_input_locked();
  pthread_mutex_unlock(&fb_lock);
}

static int keycode_present(uint8_t keycode, const struct usb_keyboard_packet *packet)
{
  int i;

  for (i = 0; i < 6; i++) {
    if (packet->keycode[i] == keycode) {
      return 1;
    }
  }
  return 0;
}

static char keycode_to_ascii(uint8_t keycode, int shifted)
{
  if (keycode >= 0x04 && keycode <= 0x1d) { /* a-z */
    return (char)('a' + (keycode - 0x04) + (shifted ? ('A' - 'a') : 0));
  }

  switch (keycode) {
  case 0x1e: return shifted ? '!' : '1';
  case 0x1f: return shifted ? '@' : '2';
  case 0x20: return shifted ? '#' : '3';
  case 0x21: return shifted ? '$' : '4';
  case 0x22: return shifted ? '%' : '5';
  case 0x23: return shifted ? '^' : '6';
  case 0x24: return shifted ? '&' : '7';
  case 0x25: return shifted ? '*' : '8';
  case 0x26: return shifted ? '(' : '9';
  case 0x27: return shifted ? ')' : '0';
  case 0x2c: return ' ';
  case 0x2d: return shifted ? '_' : '-';
  case 0x2e: return shifted ? '+' : '=';
  case 0x2f: return shifted ? '{' : '[';
  case 0x30: return shifted ? '}' : ']';
  case 0x31: return shifted ? '|' : '\\';
  case 0x33: return shifted ? ':' : ';';
  case 0x34: return shifted ? '"' : '\'';
  case 0x35: return shifted ? '~' : '`';
  case 0x36: return shifted ? '<' : ',';
  case 0x37: return shifted ? '>' : '.';
  case 0x38: return shifted ? '?' : '/';
  default: return 0;
  }
}

static int send_all(int fd, const char *buf, size_t len)
{
  size_t sent = 0;
  ssize_t n;

  while (sent < len) {
    n = write(fd, buf + sent, len - sent);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (n == 0) {
      return -1;
    }
    sent += (size_t)n;
  }

  return 0;
}

static int insert_char(char c)
{
  if (input_len >= INPUT_MAX_CHARS) {
    return 0;
  }

  memmove(&input_buf[cursor_pos + 1], &input_buf[cursor_pos], input_len - cursor_pos);
  input_buf[cursor_pos] = c;
  input_len++;
  cursor_pos++;
  input_buf[input_len] = '\0';
  return 1;
}

static int backspace_char(void)
{
  if (cursor_pos == 0) {
    return 0;
  }

  memmove(&input_buf[cursor_pos - 1], &input_buf[cursor_pos], input_len - cursor_pos);
  input_len--;
  cursor_pos--;
  input_buf[input_len] = '\0';
  return 1;
}

static int move_cursor_left(void)
{
  if (cursor_pos == 0) {
    return 0;
  }
  cursor_pos--;
  return 1;
}

static int move_cursor_right(void)
{
  if (cursor_pos >= input_len) {
    return 0;
  }
  cursor_pos++;
  return 1;
}

static void send_current_input(void)
{
  char outbound[INPUT_MAX_CHARS + 2];
  size_t outbound_len;

  if (input_len == 0) {
    return;
  }

  memcpy(outbound, input_buf, input_len);
  outbound[input_len] = '\n';
  outbound_len = input_len + 1;

  if (send_all(sockfd, outbound, outbound_len) != 0) {
    perror("Error: write failed");
    return;
  }

  pthread_mutex_lock(&fb_lock);
  append_receive_text_locked(input_buf, input_len);
  append_receive_text_locked("\n", 1);
  input_len = 0;
  cursor_pos = 0;
  input_buf[0] = '\0';
  render_input_locked();
  pthread_mutex_unlock(&fb_lock);
}

static int handle_new_key(uint8_t keycode, int shifted)
{
  char c;

  switch (keycode) {
  case 0x29: /* ESC */
    return 1;
  case 0x28: /* ENTER */
    send_current_input();
    return 0;
  case 0x2a: /* BACKSPACE */
    if (backspace_char()) {
      pthread_mutex_lock(&fb_lock);
      render_input_locked();
      pthread_mutex_unlock(&fb_lock);
    }
    return 0;
  case 0x4f: /* RIGHT */
    if (move_cursor_right()) {
      pthread_mutex_lock(&fb_lock);
      render_input_locked();
      pthread_mutex_unlock(&fb_lock);
    }
    return 0;
  case 0x50: /* LEFT */
    if (move_cursor_left()) {
      pthread_mutex_lock(&fb_lock);
      render_input_locked();
      pthread_mutex_unlock(&fb_lock);
    }
    return 0;
  default:
    c = keycode_to_ascii(keycode, shifted);
    if (c != 0 && insert_char(c)) {
      pthread_mutex_lock(&fb_lock);
      render_input_locked();
      pthread_mutex_unlock(&fb_lock);
    }
    return 0;
  }
}

static int process_keyboard_packet(const struct usb_keyboard_packet *packet,
                                   const struct usb_keyboard_packet *prev_packet)
{
  int i;
  int shifted = (packet->modifiers & (USB_LSHIFT | USB_RSHIFT)) != 0;
  uint8_t keycode;

  for (i = 0; i < 6; i++) {
    keycode = packet->keycode[i];
    if (keycode == 0) {
      continue;
    }
    if (keycode_present(keycode, prev_packet)) {
      continue;
    }
    if (handle_new_key(keycode, shifted)) {
      return 1;
    }
  }

  return 0;
}

int main()
{
  int err;
  int transferred;
  int rc;
  struct sockaddr_in serv_addr;
  struct usb_keyboard_packet packet;
  struct usb_keyboard_packet prev_packet;

  memset(&packet, 0, sizeof(packet));
  memset(&prev_packet, 0, sizeof(prev_packet));

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Open the keyboard */
  if ((keyboard = openkeyboard(&endpoint_address)) == NULL) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }

  /* Create a TCP communications socket */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if (inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  initialize_ui();

  /* Start the network thread */
  if (pthread_create(&network_thread, NULL, network_thread_f, NULL) != 0) {
    fprintf(stderr, "Error: could not create network thread\n");
    exit(1);
  }

  /* Look for and handle keypresses */
  for (;;) {
    rc = libusb_interrupt_transfer(keyboard, endpoint_address,
                                   (unsigned char *)&packet, sizeof(packet),
                                   &transferred, 0);
    if (rc == LIBUSB_ERROR_INTERRUPTED) {
      continue;
    }
    if (rc != 0) {
      fprintf(stderr, "Error: keyboard read failed: %d\n", rc);
      break;
    }
    if (transferred != sizeof(packet)) {
      continue;
    }
    if (process_keyboard_packet(&packet, &prev_packet)) {
      break;
    }
    prev_packet = packet;
  }

  shutdown(sockfd, SHUT_RDWR);
  pthread_join(network_thread, NULL);
  close(sockfd);

  if (keyboard != NULL) {
    libusb_close(keyboard);
  }
  libusb_exit(NULL);

  return 0;
}

void *network_thread_f(void *ignored)
{
  (void)ignored;

  char recvBuf[BUFFER_SIZE];
  ssize_t n;

  /* Receive data */
  for (;;) {
    n = read(sockfd, recvBuf, BUFFER_SIZE);
    if (n > 0) {
      pthread_mutex_lock(&fb_lock);
      append_receive_text_locked(recvBuf, (size_t)n);
      pthread_mutex_unlock(&fb_lock);
    } else if (n == 0) {
      break;
    } else {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
  }

  return NULL;
}
