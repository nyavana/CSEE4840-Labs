# Lab 2 Bug Fixes

Three bugs identified during the TA demo, all fixed in `lab2.c`.

---

## Fix 1: Cursor Position (Bugs 1 & 3)

**Problem:** `draw_cursor_locked()` always drew `'^'` on the divider row (row 21) using only `cursor_pos % SCREEN_COLS`. The cursor never indicated which input line (row 22 or 23) the edit position was on.

**Root cause:** The function computed only a column offset and placed the indicator on `DIVIDER_ROW`, ignoring the row component of `cursor_pos`.

**Fix (lines 203-220):** Compute both row and column from `cursor_pos`. Draw `'_'` at the actual position inside the input area (rows 22-23). The divider is restored to clean dashes on every redraw.

```c
cursor_row = INPUT_TOP_ROW + (int)(visible_pos / SCREEN_COLS);
cursor_col = (int)(visible_pos % SCREEN_COLS);
draw_divider_locked();
if (cursor_row < SCREEN_ROWS) {
  fbputchar('_', cursor_row, cursor_col);
}
```

---

## Fix 2: Key Repeat

**Problem:** Holding a key produced exactly one character. `libusb_interrupt_transfer` used timeout `0` (infinite block), and `process_keyboard_packet()` suppressed any keycode already present in the previous packet. When USB hardware repeat did leak through, shift state was re-read from the current packet, causing inconsistent capitalization (e.g., `Rrrr` instead of `RRRR`).

**Root cause:** No software typematic repeat; shift state not captured at press time.

**Fix (lines 40-42, 71-75, 79-84, 448-485, 539-573):**

- Added constants: `REPEAT_DELAY_MS` (500ms), `REPEAT_INTERVAL_MS` (50ms), `USB_TIMEOUT_MS` (50ms).
- Added state variables: `repeat_keycode`, `repeat_shifted`, `repeat_active`, `repeat_start_time`, `repeat_last_time`.
- Added `time_ms()` helper using `gettimeofday`.
- Changed `libusb_interrupt_transfer` timeout from `0` to `USB_TIMEOUT_MS`.
- On `LIBUSB_ERROR_TIMEOUT`: check repeat timer and fire `handle_new_key(repeat_keycode, repeat_shifted)` at interval after initial delay.
- Shift state is captured at initial keypress, so held Shift+R repeats as `RRRR` consistently.
- ESC (`0x29`), Enter (`0x28`), and Caps Lock (`0x39`) are excluded from repeat tracking. Backspace and arrow keys are allowed to repeat.
- Repeat stops when the tracked key is released or Enter is pressed.

---

## Fix 3: Caps Lock Support

**Problem:** Caps Lock (USB HID keycode `0x39`) was not handled. The only way to type uppercase letters was holding Shift.

**Fix (lines 69, 407-408, 431-437):**

- Added `caps_lock_active` toggle variable.
- Pressing Caps Lock toggles the state; it does not repeat when held.
- For letter keys (`0x04`-`0x1d`): `effective_shifted = shifted || caps_lock_active`.
- For numbers/symbols: `effective_shifted = shifted` (Caps Lock has no effect — Shift still required for `!@#` etc.).
- The repeat system captures effective shift at press time, so repeated letters respect the current Caps Lock state.

---

## Cross-Fix Interactions

- **Cursor vs Repeat/Caps Lock:** Independent. Cursor rendering is purely a display concern.
- **Repeat + Caps Lock:** Caps Lock is excluded from repeat tracking (would toggle rapidly otherwise). Repeated letter keys re-evaluate `caps_lock_active` inside `handle_new_key()`, so toggling Caps Lock mid-repeat correctly changes case on the next repeated character.

## Verification

1. `make clean && make` — compiles without warnings
2. Type past column 64 — cursor `_` wraps to row 23
3. Arrow keys move cursor between rows 22-23 correctly
4. Hold `r` — produces `rrrrr...` after 500ms delay
5. Hold Shift+R — produces `RRRRR...` consistently
6. Caps Lock on → letters uppercase without Shift; Caps Lock off → back to lowercase
7. Caps Lock does not affect `1` → `!` (still requires Shift)
8. ESC and Enter still work normally
