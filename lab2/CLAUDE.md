# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CSEE 4840 Lab 2: A chat client for the DE1-SoC board (ARM Cyclone V + FPGA). The program reads keystrokes from a USB keyboard via libusb, draws text on a VGA monitor via a Linux framebuffer (`/dev/fb0`), and communicates with a TCP chat server. It runs on Ubuntu 16.04.5 LTS (ARM) on the board itself, not on the workstation. 

You only need to edit `lab2.c` and keep other files intact.

## Build and Run

```bash
# Build (must run on the DE1-SoC board)
make

# Clean
make clean

# Run (requires VGA monitor, USB keyboard, and network on the board)
./lab2

# Create submission archive
make lab2.tar.gz
```

Dependencies: `gcc`, `make`, `libusb-1.0-0-dev` (install on board with `apt install`).

Linker flags: `-lusb-1.0 -pthread`. Compiler flag: `-Wall`.

## Architecture

### Threading Model

Two threads share access to the framebuffer, protected by `fb_lock` (pthread mutex):

- **Main thread** (`main()` in `lab2.c`): Polls USB keyboard via `libusb_interrupt_transfer()` in a blocking loop. Processes 8-byte HID packets (1 modifier byte + 1 reserved + 6 keycode slots). Detects new keypresses by diffing against the previous packet.
- **Network thread** (`network_thread_f()`): Blocking `read()` on the TCP socket. Appends received text to the receive region.

Functions suffixed `_locked` must be called while holding `fb_lock`.

### Screen Layout (24 rows x 64 cols)

```
Row 0..20   - Receive region (incoming + sent messages, scrolls up when full)
Row 21      - Divider line (dashes)
Row 22..23  - Input area (2 rows, max 128 chars)
```

Constants: `SCREEN_ROWS=24`, `SCREEN_COLS=64`, `INPUT_ROWS=2`, `INPUT_MAX_CHARS=128`.

### Source Files

- **`lab2.c`** — Main application. Socket setup, UI state (input buffer with cursor, receive line buffer), keyboard-to-ASCII translation, input editing (insert/backspace/arrows), message send/receive logic. `SERVER_HOST` and `SERVER_PORT` are defined here.
- **`fbputchar.c`** / **`fbputchar.h`** — Framebuffer library. `fbopen()` mmaps `/dev/fb0`. `fbputchar(char, row, col)` renders an 8x16 bitmap font at 2x scale on a 32bpp framebuffer (white on black). `fbputs()` draws a string. Font data is a static `unsigned char font[]` array embedded in the file.
- **`usbkeyboard.c`** / **`usbkeyboard.h`** — `openkeyboard()` enumerates USB devices via libusb, finds the first HID keyboard, detaches kernel driver, claims the interface, and returns the device handle + endpoint address. Defines modifier bitmasks (`USB_LSHIFT`, `USB_RSHIFT`, etc.) and the `usb_keyboard_packet` struct.

### Key Implementation Details

- USB keycodes are **not** ASCII. `keycode_to_ascii()` in `lab2.c` maps HID keycodes (0x04–0x38) to ASCII, with shift variants. See USB HID Usage Tables (page 53) for the full mapping.
- Input buffer supports mid-line editing: `cursor_pos` tracks insertion point independently of `input_len`. Arrow keys move cursor; backspace deletes at cursor; Enter sends full buffer regardless of cursor position.
- Receive region uses a circular-ish buffer (`recv_lines[RECV_ROWS][SCREEN_COLS]`). When it fills, `scroll_receive_locked()` shifts all lines up by one via `memcpy`.
- Long messages wrap automatically at `SCREEN_COLS` boundary (no word-wrap).
- ESC (keycode 0x29) exits the program cleanly: shuts down socket, joins network thread, closes libusb.
- Software key-repeat: after `REPEAT_DELAY_MS` (500ms), repeats fire every `REPEAT_INTERVAL_MS` (50ms). Shift state is captured at press time for consistent behavior (e.g., held Shift+R produces `RRRR`). ESC, Enter, and Caps Lock are excluded from repeat.
- Caps Lock toggles letter case (XOR with Shift for a-z only; does not affect number/symbol keys).
- Cursor is drawn as `'_'` at `cursor_pos` inside the input area (rows 22-23), not on the divider row.

### Documentation

- `doc/description.md` — Lab handout (assignment spec).
- `doc/fixes.md` — Documents three bug fixes from the TA demo (cursor position, key repeat, Caps Lock).
- `to-do.md` — Board setup and functional test checklist for demo readiness.

### Configuration

Edit `lab2.c` to set:
- `SERVER_HOST` — IP address string of the chat server (default: `"128.59.19.114"` = arthur.cs.columbia.edu)
- `SERVER_PORT` — port number (default: `42000`)

### Board Setup

Connect to the board's serial console: `screen /dev/ttyUSB0 115200`

Login: `root` / `CSee4840!`

Network: `ifup eth0`
