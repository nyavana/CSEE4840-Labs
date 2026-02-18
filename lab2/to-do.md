# CSEE 4840 Lab 2 Board Guide

## Quick Checklist

- `lab2.c` includes all group members' name and UNI in the header comment.
- `SERVER_HOST` in `lab2.c` is set to the chat server IP you will use.
- DE1-SoC board is booted with VGA, USB keyboard, Ethernet, and serial cable connected.
- Required packages are installed on the board.
- `make` builds successfully on the board.
- `./lab2` runs and passes all functional checks below.
- `make lab2.tar.gz` produces the submission archive.
- You are ready to demo live to a TA.

## 1. Boot and Connect

On your workstation:

```bash
screen /dev/ttyUSB0 115200
```

On the board:

```bash
ifup eth0
```

## 2. Install Dependencies (Once Per SD Image)

```bash
apt update
apt upgrade -y
apt install -y gcc make libusb-1.0-0-dev usbutils openssh-client wget
```

## 3. Configure and Build

```bash
cd ~/lab2
```

Edit `lab2.c` and set:

- `SERVER_HOST` to your target chat server IP
- name/UNI comment at the top

Then build:

```bash
make clean
make
```

## 4. Run

```bash
./lab2
```

## 5. Functional Test Checklist

Verify each item while `./lab2` is running:

- Screen is cleared at startup.
- Display is split with a horizontal divider.
- Input area is bottom two rows.
- Cursor is visible while typing.
- Typing inserts text correctly in input area.
- Left and right arrow keys move cursor correctly.
- Backspace deletes correctly, including edge cases.
- Left and right Shift keys both work for uppercase/symbols.
- Enter sends message to server and clears input area.
- Sent messages appear in top receive/history region.
- Incoming messages appear in top receive/history region.
- Long messages wrap correctly across lines.
- When receive region fills, old lines scroll up.
- Esc exits cleanly back to shell.

## 6. Demo Readiness

Run through the Section 11 demo sequence from the handout, especially:

- mid-line edits with left/right arrows
- backspace edits
- Enter pressed when cursor is not at end
- long text overflow behavior
- special key behavior

## 7. Final Deliverable

1. Confirm `lab2.c` header comment has all names and UNIs.
2. Create archive:

```bash
make lab2.tar.gz
tar tzf lab2.tar.gz
```

3. Submit `lab2.tar.gz` to Courseworks.
4. Demonstrate the working lab live to a TA during office hours.

## 8. Quick Troubleshooting

- Build fails with missing `libusb` headers:
  - install `libusb-1.0-0-dev`
- Connect fails:
  - verify server IP/port and board network connectivity
- No keyboard input:
  - reconnect USB keyboard, rerun program
- No VGA output:
  - verify boot mode switches and monitor connection
