# csee 4840

# Embedded System Design

# Lab 3: Peripherals and Device Drivers

Stephen A. Edwards

Columbia University

Spring 2025

Implement on the fpga a memory-mapped peripheral (an Avalon mm agent) that receivescommunication from the arm processors on the Cyclone V. Communicate with yourperipheral through a Linux userspace program that accesses a device driver you havewritten.

Display a ball on the vga screen with your peripheral. Implement an ioctl in your devicedrive that sends your peripheral coordinates from software.

# 1 Introduction

In this lab, you will control your own hardware from your own software, communicatingthrough a Linux device driver. We supply a base hardware design to extend, a workingexample of a vga peripheral you will have to modify, and a working device driver for itthat you will have to adapt to work with your own peripheral.

You will implement a video bouncing ball in this setting. Your peripheral will generate avga raster consisting of a ball at a particular location, your userspace C program (software)will make this ball bounce around the screen, and your device driver will mediate betweenyour program and your peripheral.

# 2 Compile the vga Component Into a New fpga Image

In this section, you will tell Platform Designer about a new peripheral component, connectit to the arm processors, and synthesize a new fpga conguration bitstream.

# 2.1 Create the vga Ball Component

Download lab3-hw.tar.gz from the class website and unpack it on your workstation. Changeto the lab3-hw directory. Run qsys-edit soc_system.qsys, which will bring up a GUI. Its fullpath is /tools/intel/intelFPGA/21.1/quartus/sopc_builder/bin/.

Create a new vga_ball component and connect it to the base design. Select File NewComponent. This should open the Component Editor window.

In the Component Type tab, set Name to vga_ball and Display Name to VGA Ball.

In the Files tab, click Add File. . . under Synthesis Files and select the vga_ball.sv le. Clickon Analyze Synthesis Files. This should quickly complete successfully; close the pop-upwindow. Set Top-Level Module to vga_ball. Some warnings and errors should appear in theMessages tab; we will x them.

# 2.2 Assign the Interface Signals on the vga Ball Component

When Platform Designer analyzes the synthesis les, it makes some good guesses aboutthe meaning of each signal on the peripheral, but it is not perfect. Fix the mistakes like this:

Click on the Signals & Interfaces tab. Click on avalon_slave_0 and set its Associated Resetto reset. Click on <<add interface>> in the left box and select Conduit (to Platform Designer,a “conduit” is an arbitrary group of signals using an unknown protocol). Set the name ofthe new conduit to vga.

Select and drag all the vga_ signals so they are under your newly created vga conduit.Click on each signal and change its Signal Type to a lowercase version of the name afterthe vga_, e.g., vga_blank_n should become blank_n. Type each of these names.

Your new component should appear in the component editor as shown below. Makesure there are no errors or warnings.

![](images/88130c213c4c5f119a7fb64828042f5b1ff216866b659f6f6206dc70fe2eb27c.jpg)


Note that this agent port’s address units are set to words, whose size is set by the(must be equal) widths of the readdata and writedata ports. In the supplied System Verilog,writedata is 8 bits wide, so the address port delivers byte addresses, just like those used bythe processor. If you change writedata to 16 bits, words become 16 bits and the address portdelivers an oset in 16-bit words. The base address continues to point to the rst register(whose address is 0), but the second register (with address 1) will appear in software at thebase address plus 2 because functions like iowrite16() use byte addresses.

Also notice that the “read wait” timing is set to 1 cycle, meaning that data being readfrom this component (not used in this lab) is expected the cycle after chipselect is asserted,as shown in the timing diagram.

Once you have eliminated all errors, click on Finish. It will warn you that it is savingvga_ball_hw.tcl; click on Yes, Save. The Component Editor window should close.

Open vga_ball_hw.tcl with a text editor and add the following three lines after the modulevga_ball section:

```tcl
set_module_assignment embeddedsw.dts.vendor "csee4840"  
setmodule_assignment embeddedsw.dts.name "vga_ball"  
setmodule_assignment embeddedsw.dts.group "vga"
```

These make the device show up as compatible with csee4840,vga_ball-1.0 in the .dtb le,which we will discuss below.

Platform Designer now knows about your custom component, so connect it to the rest ofyour design.

In Platform Designer, add an instance of the new VGA Ball component by selecting itunder “Project” in the library and clicking on the + Add. . . button. By default, it will benamed vga_ball_0.

On the new vga_ball_0 component instance, connect the clock to clk from clk_0 andconnect reset to clk_reset from clk_0.

Connect the avalon_slave_0 port on vga_ball_0 to the h2f_lw_axi_master port on thehps_0 component (this is the slower “lightweight” bus from the arm processors).

Double-click to export vga_ball_0’s vga conduit in the Export column. Set the name ofthe export to vga. This is the name Platform Designer will use in the generated code.

The System Contents tab should now look like this:

![](images/14368fc70d69a161b484a832fe4ea219f478637c70e5884765a0feaa494cd7e7.jpg)


Save the system (File Save), which should write soc_system.qsys.

Generate the Verilog for the design by clicking on Generate HDL. . . (accept the defaults)or running make qsys.

Once generating Verilog has completed without warnings or errors, click “Finish” toclose Platform Designer.

# 2.4 Connect the vga Peripheral to its Pins

Your vga Ball peripheral needs to communicate through its conduit through pins to ano-chip vga dac. To do this, edit soc_system_top.sv with a text editor to add the followingconnections within the instance of soc_system near the end of the le:

```txt
.vga_r (VGA_R),  
.vga_g (VGA_G),  
.vga_b (VGA_B),  
.vga_clk (VGA_CLK),  
.vga_hs (VGA_HS),  
.vga_vs (VGA_VS),  
.vga_blank_n (VGA_BLANK_N),  
.vga_SYNC_n (VGA_SYNC_N)
```

Platform Designer chose names like vga_blank_n by combining the name of the “Export”for the conduit (vga, in Platform Designer) with an underscore and the name of each SignalType when the conduit was dened in the Component Editor.

Delete the two assign statements to the vga signals at the bottom of soc_system_top.sv.

# 2.5 Compile the Hardware Design with Quartus

To compile the Platform Designer-generated Verilog, run make quartus. This compilesthe System Verilog source (Platform Designer places this in the soc_system directory) intoan “sram object le” output_les/soc_system.sof. To do so, it runs the soc_system.tcl scriptto create a preliminary project, then runs the Quartus mapping step to build an initialschematic that enables it to run the hps_sdram_p0_pin_assignments.tcl script generated byPlatform Designer to congure certain sdram-related pins. This process can be done inthe Quartus gui, but it’s far easier to let the provided Makele do it.

This will generate a lot of warnings. We added a list of innocuous warnings in PlatformDesigner generated code to a le called soc_system.srf. Quartus suppresses these warnings(they’re placed in “Flow Suppressed Messages”). Look carefully at any warnings that arestill being generated, especially for those from your les, such as vga_ball.sv. All of theseare logged in various .rpt les found in the output_les directory.

You only specify cycle-level timing in the synthesizable rtl subset of System Verilog: whichsignals should change in each cycle, but not their timing during a clock cycle.

We ask (in soc_system.sdc) for a “slow” 50 MHz fpga clock. Quartus does its best torestructure the logic to meet this, but it may fail on a circuit with too many gates in series.

Quartus performs Static Timing Analysis to verify the generated circuit meets this clockconstraint. This involves calculating precise delay numbers for every “gate” and “wire” inthe generated circuit and then running an all-paths longest path calculation. Such analysisis standard in modern logic design ows for fpgas and asics.

One key metric is slack: the amount of time between when data is guarateed stable andwhen the clock may come. If you have negative slack, your data may arrive too late andthe circuit is likely to produce incorrect results.

The other key, related metric is Fmax, the highest frequency allowed on a given clockbefore the circuit may start to misbehave.

You can nd the results of this analysis in two places. If you run the Quartus gui, theresults of static timing analysis is available in one of the many report windows. For example,Fmax is 113 MHz for the skeleton design provided, much higher than the 50 MHz requested.

![](images/13ac3f7495bba060c988dc04722b26e31dcc545dda7dc4b2d3a283d7a1bc0160.jpg)


This number is also reported in output_les/soc_system.sta.rpt:

```txt
; Slow 1100mV 85C Model Fmax Summary  
+  
; Fmax ; Restricted Fmax ; Clock Name  
+  
; 113.19 MHz ; 113.19 MHz ; clock_50_1  
; 1184.83 MHz ; 717.36 MHz ; soc_system:soc_system0|soc_system_hps_0:hps_0|soc_
```

# 2.7 Copy soc_system.rbf To Your SD Card

After Quartus nishes compiling, convert the .sof le to an .rbf le by running make rbf.

Copy the output_les/soc_system.rbf into the boot partition of your sd card. You canmount your sd card on your workstation and copy the le. Alternatively, mount the bootpartition by running mount /dev/mmcblk0p1 /mnt on your DE1-SoC then use scp to copythe le from your workstation to your board, e.g.,

```batch
scp sedwards@micro11.ee.columbia.edu:lab3/soc_system.rbf /mnt
```

Ensure the le has actually been written out to the card: type sync at the command-line.

To isolate hardware from software problems, you can manually exercise your peripheral’sregisters using u-boot, the rst stage bootloader. For example, after copying your .rbf leto the boot partition, connect your board to your workstation via a mini-usb cable, runscreen /dev/ttyUSB0 115200, boot your fpga board, and quickly press a key (such as space)to enter the u-boot command line:

```txt
U-Boot SPL 2013.01.01 (Jan 12 2019 - 19:40:48) BOARD : Altera SOCFPGA Cyclone V Board CLOCK: EOSC1 clock 25000 KHz CLOCK: EOSC2 clock 25000 KHz ... U-Boot 2013.01.01 (Jan 12 2019 - 19:41:00) CPU : Altera SOCFPGA Platform ... Warning: failed to set MAC address Hit any key to stop autobot: 0 SOCFPGA_CYCLONE5 #
```

Unless you rebooted from Linux, the fpga is not yet congured, so read the .rbf le, useit to congure the fpga, and enable the bus bridges:

```perl
SOCFPGA_CYCLONE5 # fatloadmmc0:1 $fpgadata soc_system.rbf
reading soc_system.rbf
7007204 bytes read in 333 ms (20.1MiB/s)
SOCFPGA_CYCLONE5 # fpga load 0 $fpgadata $filesize
SOCFPGA_CYCLONE5 # run bridge_enable_handoff
## Starting application at 0x3FF79598 ...
## Application terminated, rc = 0x0
```

Now, you can issue memory write commands to modify registers. Platform Designer putmy vga_ball agent at address ff20 0000, so you can set the red, green, and blue componentsof the background color using the “memory write” command:

```asm
SOCFPGA_CYCLONE5 # mw.b ff200000 70  
SOCFPGA_CYCLONE5 # mw.b ff200001 d9  
SOCFPGA_CYCLONE5 # mw.b ff200002 b3
```

The base addresses of fpga peripherals begin at ff20 0000, which is the base addressof the bus bridge. Each peripheral has its own oset beyond that. Base addresses and/orosets can be found in Platform Designer, in the soc_system.dts le, in the /proc/device-treedirectory, or in /proc/iomem if your kernel driver is installed.

# 3 Tell the Linux Kernel About Your Peripheral Through the Device Tree

The Linux kernel employs a persistent data structure known as the Device Tree to describethe structure of a hardware platform. It contains information about processors, memoryregions, bus bridges, and most importantly, the types and memory location of peripheralssuch as the vga Ball. Platform Designer generates a similar soc_system.sopcinfo le that,in concert with the soc_system_board_info.xml le, can be used to generate an apporiatesoc_system.dtb le, a binary representation of the Device Tree that is normally loaded aspart of the boot process.

Run embedded_command_shell.sh to add sopc2dts and dtc to your path and then generatesoc_system.dtb by running make dtb. These programs are part of the Intel SoC fpgaEmbedded Development Suite, which is a separate download from the Intel Quartus website.

Verify that the vga Ball peripheral appears in the soc_system.dts le, which should include

```html
vga Ball 0: vga@0x10000000 { compatible = "csee4840, VGA Ball-1.0"; reg = <0x00000001 0x00000000 0x00000008>; clocks = <&clk_0>; }; //end VGA@0x100000000 (vga Ball_0)
```

The entry itself comes from the vga_ball_0 module instance in Platform Designer(soc_system.qsys). The compatible string is controlled by the set_module_assignment state-ments you should have added to the vga_ball_hw.tcl le.

As you did for the .rbf le, copy the soc_system.dtb le to your sd card’s boot partition.

# 4 Communicate with Your Peripheral Through Software

Connect the console port on your DE1-SoC board (via the mini-usb cable) to your worksta-tion and run screen /dev/ttyUSB0 115200 as you did in lab 2.

Connect a vga monitor to your DE1-SoC. Boot Linux on your board from the sd cardwith your new soc_system.rbf and soc_system.dtb les (your sd card from lab2 is otherwisene). If your board is already powered on, restart it by typing reboot (don’t power-cycle it).

Boot Linux on your board. It should go through the normal boot process and you shouldsee a white box against a colored background on the vga monitor.

Verify that the kernel sees the vga Ball device in the device tree:

```shell
ls "/proc/device-tree/sopc@0/bridge@0xc0000000/"  
#address-cells clock-names compatible ranges reg-names  
#size-cells clocks name reg vga@0x100000000  
#cat "/proc/device-tree/sopc@0/bridge@0xc0000000/vga@0x100000000/compatible"  
csee4840, vgaBall-1.0
```

# 4.1 Compile and Run the Sample Program

On your board, download and install linux-headers-4.19.0.tar.gz, which includes the Makelefor compiling kernel modules.

```shell
# wget https://www.cs.columbia.edu/~sedwards/classes/2025/4840-spring/linux-headers-4.19.0.tar.gz
# tar Pzxf linux-headers-4.19.0.tar.gz
# ls /usr/src/linux-headers-4.19.0
Documentation arch drivers init mm scriptsUSR
Kconfig block firmware ipc modules.order security virt
Makefile certfs kernel net sound
Module.symvers crypto include lib samples tools
```

Install the kernel module mangement programs (e.g., insmod, rmmod).

```txt
apt install -y kmod
```

Download lab3-sw.tar.gz from the class website to your board, unpack it, compile it,install the kernel module.

```shell
# wget https://www.cs.columbia.edu/~sedwards/classes/2025/4840-spring/lab3-sw.tar.gz
# tar zxf lab3-sw.tar.gz
# cd lab3-sw
```

Compile the device driver and user program, install the kernel module, and verify that itworks. This should look like

```txt
# make   
make -C /usr/src/linux-headers-4.19.0 SUBDIRS=/root/lab3 modules make[1]: Entering directory '/usr/src/linux-headers-4.19.0' CC [M] /root/lab3/vga Ball.o Building modules, stage 2. MODPOST 1 modules CC /root/lab3/vga ball.mod.o LD [M] /root/lab3/vga ball.ko   
make[1]: Leaving directory '/usr/src/linux-headers-4.19.0' cc hello.c -o hello # insmod vga ball.ko # lsmod Module Size Used by vga ball 16384 0 # ./hello   
VGA ball Userspace program started initial state: f9 e4 b7 ff 00 00 00 ff 00 00 00 ff 00 ff 00 ... ff 00 ff VGA BALL Userspace program terminating # rmmod vga ball rmmod: ERROR: ../libkmod/libkmod.c:514 lookup_builtin_file() could not open builtin file '/lib/modules/4.19.0/modules.builtin.bin'
```

You may ignore the error from rmmod.

make compiles the kernel module (vga_ball.ko) and the userspace program (hello).

insmod loads the generated kernel module. In the supplied device driver, doing thisshould change the display. lsmod lists installed modules.

The hello program is a userspace program that communicates with the vga_ball devicedriver through the ioctl system call. It opens the device and reads and writes its state,which changes the color of the background.

rmmod removes the kernel module, which is necessary any time you modify and re-compile the module.

# What to Do

Modify the hardware and software in the skeleton you have been provided to displaya bouncing ball. Change both the interface and contents of the hardware peripheral sothat it displays a stationary ball at a software-controllable set of coordinates. Have yourperipheral respond to writes to one or more addresses that control the location of the ball.

The register map for the provided vga ball component consists of three single-byteregisters, one for each color:

# Oset 7 · · · 0 Meaning

<table><tr><td>0</td><td>Red</td><td>Red component of background color (0-255)</td></tr><tr><td>1</td><td>Green</td><td>Green component of background color (0-255)</td></tr><tr><td>2</td><td>Blue</td><td>Blue component of background color (0-255)</td></tr></table>

Change this register map so that you can convey $( x , y )$ coordinates of your ball to thehardware. You may modify the width of the agent interface (this is the writedata port invga_ball.sv; it is currently 8 bits, but you may want to use 16 or 32) and the number ofregisters.

Update the comment in vga_ball.sv to reect your new register map.

Record the most conservative Fmax of your new peripheral (Slow 1100 mV, 85 C) andmake sure it is above the required 50 MHz.

Adapt the provided device driver to communicate with your peripheral. E.g., create anioctl that sets the coordinates of the ball.

Write a userspace program that bounces the ball by repeatedly communicating the newcoordinates to your peripheral through your device driver.

You may observe that your ball “tears” as it moves across the screen. This is caused bychanging the ball’s coordinates while one of its lines is being generated. To x this, makeit so that your ball’s coordinates only change when other lines are being displayed.

# 6 What to turn in

Find an overworked TA and show him your bouncing ball, your updated register mapinformation in a comment in vga_ball.sv, and the Fmax of your completed project. Oncehe is satised, collect just the les you wrote or modied for this lab in a directory called“lab3,” make a tarball with tar zcf lab3.tar.gz lab3, and submit that via Courseworks. Thisshould include the SystemVerilog for your peripheral and C source for your device driverand userspace program.

Do not submit everything in your lab3-hw directory: it is too big.

# 7 Platform Designer Hints

# 7.1 Editing the Source of Your Platform Designer Component

If you modify the SystemVerilog for your hardware component without changing itsinterface, regenerate your system with Platform Designer then re-run Quartus. Do this byrunning make qsys-clean ; make qsys or open Platform Designer from Quartus (Tools Qsys)and click on Generate HDL. . . .

If you modify the interface your hardware component (e.g., to change the number ofvisible registers, add a read function, or change the signals passed through the conduit),edit the component. Start Platform Designer (e.g., run qsys-edit), open your .qsys le, selectyour component under “Project,” and click “Edit.” This should bring up the ComponentEditor window.

Re-analyze the synthesis les as you did in Section 2.1, make sure the interface signalsare assigned correctly, and click Finish.

Every time you update the compoent, re-insert the set_module_assignment directivesmentioned in Section 2.2.

In Platform Designer, select File Refresh System (or just press F5). It should completewith a reassuring warning indicating the version of your component has changed. Hoveringover the instance of your component should also indicate its version has changed. Saveyour project after doing this to update the .qsys le.

Now, select Generate Generate. . . to instruct Platform Designer to regenerate yoursystem so Quartus can recompile it. Alternately, run make qsys-clean ; make qsys, whichdoes the same thing from the command line.

# 7.2 Don’t Edit Copies

Do not edit the les in the synthesis directory (e.g., in lab3-hw/synthesis/submodules). These are copied by Platform De-signer and will be overwritten the next time Platform De-signer runs.

# 7.3 Viewing Components as Blocks

Select a component and then View Block Symbol. Thisshows how Platform Designer interprets the interface to acomponent.

![](images/04ddcd378e7a9fa471bf8e426061e63b179a733594db778dbc583cca451575f5.jpg)
