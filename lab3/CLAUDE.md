# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CSEE 4840 Lab 3: Peripherals and Device Drivers for the DE1-SoC (Cyclone V FPGA + ARM HPS). The goal is to display a bouncing ball on a VGA screen controlled by an ARM Linux userspace program communicating through a custom kernel device driver to a memory-mapped FPGA peripheral.

The project description is located at doc/project_description.md

### What to Do

Modify the hardware and software in the skeleton you have been provided to display a bouncing ball. Change both the interface and contents of the hardware peripheral so that it displays a stationary ball at a software-controllable set of coordinates. Have your peripheral respond to writes to one or more addresses that control the location of the ball.

The register map for the provided vga ball component consists of three single-byte registers, one for each color:

# Offset 7 · · · 0 Meaning

<table><tr><td>0</td><td>Red</td><td>Red component of background color (0-255)</td></tr><tr><td>1</td><td>Green</td><td>Green component of background color (0-255)</td></tr><tr><td>2</td><td>Blue</td><td>Blue component of background color (0-255)</td></tr></table>

Change this register map so that you can convey $( x , y )$ coordinates of your ball to the hardware. You may modify the width of the agent interface (this is the writedata port in vga_ball.sv; it is currently 8 bits, but you may want to use 16 or 32) and the number of registers.

Update the comment in vga_ball.sv to reflect your new register map.

Record the most conservative Fmax of your new peripheral (Slow 1100 mV, 85 C) and make sure it is above the required 50 MHz.

Adapt the provided device driver to communicate with your peripheral. E.g., create an ioctl that sets the coordinates of the ball.

Write a userspace program that bounces the ball by repeatedly communicating the new coordinates to your peripheral through your device driver.

You may observe that your ball “tears” as it moves across the screen. This is caused by changing the ball’s coordinates while one of its lines is being generated. To fix this, make it so that your ball’s coordinates only change when other lines are being displayed.

## Architecture

The system has three layers:

1. **FPGA Hardware** (`hw/vga_ball.sv`) — Avalon memory-mapped agent generating VGA output. Connected to the HPS via the lightweight AXI bridge (`h2f_lw_axi_master`). Contains `vga_ball` (register interface + pixel logic) and `vga_counters` (640x480 @ 50 MHz timing, hcount is 0-1599, pixel columns are `hcount[10:1]`).

2. **Linux Kernel Driver** (`sw/vga_ball.c`, `sw/vga_ball.h`) — Platform device driver using the misc subsystem. Maps hardware registers via `of_iomap`, exposes `/dev/vga_ball` with ioctl interface (`VGA_BALL_WRITE_BACKGROUND`, `VGA_BALL_READ_BACKGROUND`). Matches device tree compatible string `"csee4840,vga_ball-1.0"`.

3. **Userspace Program** (`sw/hello.c`) — Opens `/dev/vga_ball` and uses ioctls to control the peripheral. This is where bouncing ball position logic goes.

The top-level FPGA wrapper is `hw/soc_system_top.sv`, which instantiates `soc_system` (Platform Designer-generated) and wires VGA conduit signals to board pins.

## Build Commands

### Hardware (run from `hw/`)

```bash
make qsys          # Generate Qsys system (sopcinfo, QIP, Verilog)
make quartus        # Compile FPGA design (produces .sof)
make rbf            # Convert .sof to .rbf for SD card boot
make dtb            # Generate device tree blob (requires embedded_command_shell.sh)
make qsys-clean     # Clean Qsys-generated files (needed before regenerating after interface changes)
```

After interface changes to `vga_ball.sv`: `make qsys-clean && make qsys` then `make quartus`.

### Software (run from `sw/`, on the DE1-SoC board)

```bash
make                # Builds both vga_ball.ko (kernel module) and hello (userspace program)
make module         # Build just the kernel module
make clean          # Clean build artifacts
```

Requires linux-headers at `/usr/src/linux-headers-4.19.0/` on the board (extracted from `orginal/4840_lab3_linux-headers-4.19.0.tar.gz`).

### Deployment to Board

1. Copy `hw/output_files/soc_system.rbf` and `hw/soc_system.dtb` to SD card boot partition
2. Boot the board, connect via `screen /dev/ttyUSB0 115200`
3. `insmod vga_ball.ko` then `./hello`

## Key Details

- The Avalon agent currently uses 8-bit `writedata` with byte addressing. Widening to 16/32 bits changes the address-to-register mapping (word addressing).
- VGA timing: 640x480, pixel clock 25 MHz (derived from 50 MHz via `hcount[0]`). Active region: hcount 0-1279, vcount 0-479.
- The starter code displays a fixed white rectangle; the task is to replace it with a ball at software-controllable (x,y) coordinates.
- To avoid screen tearing, update ball coordinates only during non-active display lines (vertical blanking or lines not currently rendering the ball).
- Peripheral base addresses start at `0xff200000`; offsets are visible in Platform Designer or `/proc/iomem`.
- After editing `vga_ball_hw.tcl` (auto-generated by Platform Designer), re-add the `set_module_assignment embeddedsw.dts*` lines — they get overwritten on regeneration.
- Do NOT edit files under `hw/soc_system/synthesis/submodules/` — Platform Designer overwrites them.
- Design must meet the 50 MHz Fmax constraint (check Slow 1100mV 85C corner in timing reports).
