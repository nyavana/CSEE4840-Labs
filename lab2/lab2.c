/*
 * CSEE 4840 Lab 2
 *
 * Name/UNI:
 * By: Hao Cai , Chenhao Yang
 * Uni: hc3612 , cy2822
 */

/* framebuffer font rendering and USB keyboard HID interface */
#include "fbputchar.h"
#include "usbkeyboard.h"

/* inet_pton() for IP conversion, socket API for TCP communication */
#include <arpa/inet.h>
#include <sys/socket.h>

/* separate thread for blocking network reads */
#include <pthread.h>

/* General stuff */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * Server connection settings.
 * SERVER_HOST is arthur.cs.columbia.edu.
 * SERVER_PORT is the TCP port the server listens on.
 */

/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

/*
 * Screen geometry.
 * The VGA framebuffer is in fbputchar().
 * The display is organized as 24 x 64:
 *   - RECV_TOP_ROW RECV_BOTTOM_ROW (rows 0–20): receive/display area
 *   - DIVIDER_ROW (row 21): separator
 *   - INPUT_TOP_ROW SCREEN_ROWS-1 (rows 22–23): input area
 */
#define BUFFER_SIZE 128
#define SCREEN_ROWS 24
#define SCREEN_COLS 64
#define INPUT_ROWS 2

#define INPUT_TOP_ROW (SCREEN_ROWS - INPUT_ROWS)  /* Row 22 */
#define DIVIDER_ROW (INPUT_TOP_ROW - 1)            /* Row 21 */
#define RECV_TOP_ROW 0                             /* Row 0  */
#define RECV_BOTTOM_ROW (DIVIDER_ROW - 1)          /* Row 20 */
#define RECV_ROWS (RECV_BOTTOM_ROW - RECV_TOP_ROW + 1) /* 21 rows */

/* Maximum characters the user can type before sending */
#define INPUT_MAX_CHARS (INPUT_ROWS * SCREEN_COLS)

/*
 * key-repeat timing.
 * After holding a key for REPEAT_DELAY_MS, it repeats every REPEAT_INTERVAL_MS.
 * USB_TIMEOUT_MS should be short enough to keep it responsive.
 */
#define REPEAT_DELAY_MS   500
#define REPEAT_INTERVAL_MS 50
#define USB_TIMEOUT_MS     50

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

/*
 * networking and USB
 */
int sockfd = -1; /* TCP socket file descriptor for the chat server */

struct libusb_device_handle *keyboard = NULL; /* keyboard handle */
uint8_t endpoint_address;                     /* interrupt */

/*
 * Threading. fb_lock protects all framebuffer writes
 */
pthread_t network_thread;
pthread_mutex_t fb_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Receive region state.
 * recv_lines[][] is a logical buffer of text lines displayed in rows 0–20.
 * recv_row/recv_col track the current write position. When recv_row reaches
 * RECV_ROWS, all lines scroll up by one (oldest line is discarded).
 */
static char recv_lines[RECV_ROWS][SCREEN_COLS];
static int recv_row = 0;
static int recv_col = 0;

/*
 * Input buffer state.
 * cursor_pos tracks the insertion point independently of input_len, allowing arrow keys + backspace to edit
 * anywhere within the typed text.
 */
static char input_buf[INPUT_MAX_CHARS + 1];
static size_t input_len = 0;
static size_t cursor_pos = 0;

/* Caps Lock toggle */
static int caps_lock_active = 0;

/*
 * Software key-repeat.
 * Tracks which key is held, when it was first pressed (repeat_start_time),
 * and when the last repeat event fired (repeat_last_time). After
 * REPEAT_DELAY_MS of holding, repeats fire every REPEAT_INTERVAL_MS.
 */
static uint8_t repeat_keycode = 0;
static int     repeat_shifted = 0;
static int     repeat_active = 0;
static long    repeat_start_time = 0;
static long    repeat_last_time = 0;

void *network_thread_f(void *);

/* Returns current wall-clock time in milliseconds. for key-repeat timing. */
static long time_ms(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/*
 * Screen Drawing
 */

/* Overwrite an entire screen row with spaces */
static void clear_row_locked(int row)
{
  int col;

  for (col = 0; col < SCREEN_COLS; col++) {
    fbputchar(' ', row, col);
  }
}

/* Clear all 24 rows of the text display */
static void clear_screen_locked(void)
{
  int row;

  for (row = 0; row < SCREEN_ROWS; row++) {
    clear_row_locked(row);
  }
}

/* Draw the horizontal divider line with dashes */
static void draw_divider_locked(void)
{
  int col;

  for (col = 0; col < SCREEN_COLS; col++) {
    fbputchar('-', DIVIDER_ROW, col);
  }
}

/*
 * Render one logical line from recv_lines[] onto the corresponding screen row.
 * logical_row 0 maps to screen row RECV_TOP_ROW (0), logical_row N maps to
 * screen row N.
 */
static void render_receive_row_locked(int logical_row)
{
  int col;

  for (col = 0; col < SCREEN_COLS; col++) {
    fbputchar(recv_lines[logical_row][col], RECV_TOP_ROW + logical_row, col);
  }
}

/* Clear a logical receive line by fill with spaces and redraw it on screen */
static void clear_receive_row_locked(int logical_row)
{
  memset(recv_lines[logical_row], ' ', SCREEN_COLS);
  render_receive_row_locked(logical_row);
}

/* Redraw the entire receive region from the recv_lines buffer */
static void redraw_receive_region_locked(void)
{
  int row;

  for (row = 0; row < RECV_ROWS; row++) {
    render_receive_row_locked(row);
  }
}

/*
 * Scroll the receive region up by one line.
 * Shift every line up via memcpy
 * redraw the entire region.
 * When printing reaches the bottom of the area, scroll thementry region so there is always a blank line at the bottom.
 */
static void scroll_receive_locked(void)
{
  int row;

  for (row = 1; row < RECV_ROWS; row++) {
    memcpy(recv_lines[row - 1], recv_lines[row], SCREEN_COLS);
  }
  memset(recv_lines[RECV_ROWS - 1], ' ', SCREEN_COLS);
  redraw_receive_region_locked();
}

/*
 * Move to the next line in the receive region.
 * If we've reached the bottom, scroll everything up first.
 */
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

/*
 * Append text
 * Handles special cases
 *   - '\r' is skipped
 *   - '\n' advances to the next line
 *   - strange chars are replaced with '?'
 *
 * Used for both incoming server messages and echoing the user's own sent text.
 */
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
      advance_receive_line_locked(); /* column exceeded row width */
    }

    recv_lines[recv_row][recv_col] = c;
    fbputchar(c, RECV_TOP_ROW + recv_row, recv_col);
    recv_col++;

    if (recv_col >= SCREEN_COLS) {
      advance_receive_line_locked(); /* column exceeded row width */
    }
  }
}

/*
 * Input Area Rendering
 */

/* Clear the input rows on the framebuffer */
static void clear_input_rows_locked(void)
{
  int row;

  for (row = INPUT_TOP_ROW; row < SCREEN_ROWS; row++) {
    clear_row_locked(row);
  }
}

/*
 * Draw the cursor as an underscore ('_') at the current cursor_pos.
 * The cursor position maps into the 2-row input area:
 *   row = INPUT_TOP_ROW + (cursor_pos / SCREEN_COLS)
 *   col = cursor_pos % SCREEN_COLS
 */
static void draw_cursor_locked(void)
{
  size_t visible_pos = cursor_pos;
  int cursor_row, cursor_col;

  if (visible_pos >= INPUT_MAX_CHARS) {
    visible_pos = INPUT_MAX_CHARS - 1;
  }

  cursor_row = INPUT_TOP_ROW + (int)(visible_pos / SCREEN_COLS);
  cursor_col = (int)(visible_pos % SCREEN_COLS);

  draw_divider_locked();

  if (cursor_row < SCREEN_ROWS) {
    fbputchar('_', cursor_row, cursor_col);
  }
}

/*
 * Render the full input area:
 */
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

/*
 * Initialize the entire UI to a clean state.
 * clear buffer, resets input state, clears the screen,
 * draws the divider, renders the empty input area with cursor.
 */
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

/*
 * Check whether a given keycode appears in the 6-keycode slots to detect newly pressed vs held keys.
 */
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

/*
 * Convert a USB HID to an ASCII character.
 */
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

/*
 * loop until all bytes are sent
 * returns 0 on success, -1 on error.
 */
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

/*
 * Insert a character at cursor_pos within the input buffer.
 */
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

/*
 * Delete the character to the left of cursor_pos 
 */
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

/* Move cursor one position left. Returns 0 at boundary. */
static int move_cursor_left(void)
{
  if (cursor_pos == 0) {
    return 0;
  }
  cursor_pos--;
  return 1;
}

/* Move cursor one position right. Returns 0 past input_len. */
static int move_cursor_right(void)
{
  if (cursor_pos >= input_len) {
    return 0;
  }
  cursor_pos++;
  return 1;
}

/*
 * Send the current input buffer to the chat server 
 * next echo it.
 */
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
  input_len = 0;
  cursor_pos = 0;
  input_buf[0] = '\0';
  repeat_active = 0;
  render_input_locked();
  pthread_mutex_unlock(&fb_lock);
}

/*
 * Special keys:
 *   0x29 (ESC)       — returns 1 to signal program exit
 *   0x28 (Enter)     — sends current input to server
 *   0x39 (Caps Lock) — toggles caps_lock_active
 *   0x2a (Backspace) — deletes character left of cursor
 *   0x4f (Right)     — moves cursor right
 *   0x50 (Left)      — moves cursor left
 */
static int handle_new_key(uint8_t keycode, int shifted)
{
  char c;
  int effective_shifted;

  switch (keycode) {
  case 0x29: /* ESC */
    return 1;
  case 0x28: /* ENTER */
    send_current_input();
    return 0;
  case 0x39: /* CAPS LOCK */
    caps_lock_active = !caps_lock_active;
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
    /* For letter keys, Caps Lock toggles uppercase 
     * XOR with Shift */
    if (keycode >= 0x04 && keycode <= 0x1d) {
      effective_shifted = shifted || caps_lock_active;
    } else {
      effective_shifted = shifted;
    }
    c = keycode_to_ascii(keycode, effective_shifted);
    if (c != 0 && insert_char(c)) {
      pthread_mutex_lock(&fb_lock);
      render_input_locked();
      pthread_mutex_unlock(&fb_lock);
    }
    return 0;
  }
}

/*
 * format:
 *   byte 0: special key Shift, Ctrl, Alt, L/R
 *   byte 1: always 0
 *   bytes 2–7: up to 6 simultaneous codes 
 */
static int process_keyboard_packet(const struct usb_keyboard_packet *packet,
                                   const struct usb_keyboard_packet *prev_packet)
{
  int i;
  int shifted = (packet->modifiers & (USB_LSHIFT | USB_RSHIFT)) != 0;
  repeat_shifted = shifted;
  uint8_t keycode;
  long now;

  /* Process newly pressed keys (in current packet but not in previous) */
  for (i = 0; i < 6; i++) {
    keycode = packet->keycode[i];
    if (keycode == 0) {
      continue;
    }
    if (keycode_present(keycode, prev_packet)) {
      continue; /* held key, repeat handled by timer */
    }
    if (handle_new_key(keycode, shifted)) {
      return 1;
    }
    /* Start repeat tracking , exclude ESC, Enter, Caps Lock */
    if (keycode != 0x29 && keycode != 0x28 && keycode != 0x39) {
      now = time_ms();
      repeat_keycode = keycode;
      repeat_shifted = shifted;
      repeat_active = 1;
      repeat_start_time = now;
      repeat_last_time = now;
    }
  }

  /* if the key being tracked for repeat was released, stop repeating */
  if (repeat_active && !keycode_present(repeat_keycode, packet)) {
    repeat_active = 0;
  }

  return 0;
}

/*
 * Initialization
 *   1. Open the framebuffer (/dev/fb0) for VGA text rendering
 *   2. Open the USB keyboard via libusb
 *   3. Create a TCP socket and connect to the chat server
 *   4. Initialize the UI
 *   5. network thread for receiving incoming messages
 *
 * Then enters the main keyboard polling loop:
 *   - Calls libusb_interrupt_transfer()
 *   - checks the software key-repeat timer
 *   - processes the HID packet to detect new keypresses
 *
 * shutdown by ESC:
 *   - shutdown() the socket
 *   - Join the network thread
 *   - Close the socket and libusb resources
 */
int main()
{
  int err;
  int transferred;
  int rc;
  struct sockaddr_in serv_addr;
  struct usb_keyboard_packet packet;
  struct usb_keyboard_packet prev_packet;

  /* Initialize previous packet to all zeros */
  memset(&packet, 0, sizeof(packet));
  memset(&prev_packet, 0, sizeof(prev_packet));

  /* 1 Open the framebuffer for VGA rendering */
  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* 2 Open the USB keyboard */
  if ((keyboard = openkeyboard(&endpoint_address)) == NULL) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }

  /* 3 Create a TCP socket and connect to the chat server */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Convert server IP string and set port */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if (inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect to the chat server */
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* 4 Initialize the UI */
  initialize_ui();

  /* 5 Start the network thread for receiving incoming messages */
  if (pthread_create(&network_thread, NULL, network_thread_f, NULL) != 0) {
    fprintf(stderr, "Error: could not create network thread\n");
    exit(1);
  }

  /*
   * keyboard polling loop
   * libusb_interrupt_transfer() blocks for up to USB_TIMEOUT_MS.
   * On timeout, check the software key-repeat timer.
   * On success, process the HID packet.
   */
  for (;;) {
    rc = libusb_interrupt_transfer(keyboard, endpoint_address,
                                   (unsigned char *)&packet, sizeof(packet),
                                   &transferred, USB_TIMEOUT_MS);
    if (rc == LIBUSB_ERROR_INTERRUPTED) {
      continue;
    }
    if (rc == LIBUSB_ERROR_TIMEOUT) {
      /*
       * check software key-repeat.
       * If a key has been held for >= REPEAT_DELAY_MS, repeats
       * every REPEAT_INTERVAL_MS until the key is released.
       */
      if (repeat_active) {
        long now = time_ms();
        long elapsed = now - repeat_start_time;
        if (elapsed >= REPEAT_DELAY_MS &&
            (now - repeat_last_time) >= REPEAT_INTERVAL_MS) {
          if (handle_new_key(repeat_keycode, repeat_shifted)) {
            break;
          }
          repeat_last_time = now;
        }
      }
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

  /*
   * close the socket,
   * wait for the network
   */
  shutdown(sockfd, SHUT_RDWR);
  pthread_join(network_thread, NULL);
  close(sockfd);

  if (keyboard != NULL) {
    libusb_close(keyboard);
  }
  libusb_exit(NULL);

  return 0;
}

/*
 * Network
 */
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
