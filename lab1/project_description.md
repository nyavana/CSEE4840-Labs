# csee 4840Embedded System Design Lab 1: Using the fpga
Stephen A. Edwards, Columbia University
Spring 2025
Learn how to code in SystemVerilog, run the Verilator simulator, observe simulated wave-forms with GTKWave, and compile and download an fpga-only project to the DE1-SoCboard. You will create a system that tests the Collatz conjecture over a range of values.
For this lab, you will use the open-source Verilator SystemVerilog simulator, the open-source GTKWave waveform viewer, and Quartus Prime 21.1 fpga design software producedby Intel/Altera for their chips. You can find this software on the workstations in 1235 Mudd(on which you should have an account if you registered for the class), you may be able torun it on your laptop, or use some combination of both. Quartus Prime 21.1 Lite is freeto download from the Intel website https://www.intel.com/content/www/us/en/products/details/fpga/development-tools/quartus-prime/resource.html, although you will haveto register for a free account and it only runs under Windows or Linux. While there arenewer versions available, we suggest you stick to 21.1 for consistency.
To submit this assignment do two things:
1. Put your SystemVerilog code files (hex7seg.sv, collatz.sv, range.sv, and lab1.sv) into a.tar.gz file (e.g., run make lab1.tar.gz) and upload it to Courseworks.
2. Demonstrate your working system to a ta. See Section 10 for details.
# 1 Download and Unpack the Lab 1 files
Download lab1.tar.gz from the class website1 and extract it with tar zxf lab1.tar.gz. Thiswill create a lab1 directory containing the files listed below.
<table><tr><td>Name</td><td>Contents</td></tr><tr><td>Makefile</td><td>Commands for creating the Quartus project files, compiling, building the lab1.tar.gz file, cleaning up, and running Verilator.</td></tr><tr><td>hex7seg.sv</td><td>A module skeleton for a hex-to-seven segment decoder for displaying numbers on the board</td></tr><tr><td>collatz.sv</td><td>A module skeleton for computing the Collatz iteration for a particular number.</td></tr><tr><td>range.sv</td><td>A module skeleton for computing the Collatz iteration over a range of numbers and storing the iteration counts in a small memory.</td></tr><tr><td>lab1.sv</td><td>A module skeleton that provides a user interface to the modules above.</td></tr><tr><td>hex7seg.cpp</td><td>A Verilator test bench for the hex7seg module.</td></tr><tr><td>collatz.cpp</td><td>A Verilator test bench for the collatz module, which, when working, prints a Collatz sequence from a particular value.</td></tr><tr><td>range.cpp</td><td>A Verilator test bench for the range module, which, when working, runs the collatz module over a range of numbers and stores the result in a small memory.</td></tr><tr><td>collatz.gitw</td><td>A GTXWave “save” file that remembers what signals to display, etc. for the collatz module.</td></tr><tr><td>range.gitw</td><td>A GTXWave “save” file for the range module.</td></tr><tr><td>range-done.gitw</td><td>A GTXWave “save” file that displays what should be the end behavior of the range module.</td></tr><tr><td>de1-soc-project.tcl</td><td>A Tcl script that creates the lab1 project files. Includes pin assignments.</td></tr></table>
Modify the hex7seg.sv, collatz.sv, range.sv, and lab1.sv files.
You may modify any other files; we will not grade them.
1wget https://www.cs.columbia.edu/~sedwards/classes/2025/4840-spring/lab1.tar.gz
# 2 Implement and Test a Hex-to-Seven-Segment Decoder
The DE1-SoC includes six seven-segment displays, which wewill use to display hexadecimal numbers. Each segment is con-nected to its own pin. These segment signals are active-low: a$" 0 "$ turns them on. Bit 0 (the rightmost) of each 7-bit segmentvector is the “a” segment, bit 1 is the “b” segment, etc., up tobit 6, the leftmost, which controls the “g” segment.
The hex7seg.sv file includes the interface to this module:
```txt
module hex7seg( input logic [3:0] a, output logic [6:0] y);
```
![image](/03daae47413990eb2fe7d107a37cb0fd247f0c1c61159c5d345b1d25f8735b0e.jpg)

Implement the body of the sevent-segment decoder module in hex7seg.sv. We have provideda Verilator testbench to test your implementation. Make sure Verilator is installed andcompile and run the simulation:
```txt
$ make hex7seg
0 40 OK
1 79 OK
2 24 OK
3 30 OK
4 19 OK
5 12 OK
6 02 OK
7 78 OK
8 00 OK
9 10 OK
a 08 OK
b 03 OK
c 46 OK
d 21 OK
e 06 OK
f 0e OK
SUCCEED
```
which shows the 16 possible inputs and their outputs. An incorrect output will produce
```txt
7 7a INCORRECT expected 78  
8 00 OK  
FAILED
```
Run make lint to have Verilator run a fast, thorough check on your SystemVerilog code.The solution you eventually submit should not report any lint errors.
# 3 The Collatz Conjecture
The Collatz Conjecture is that for any positive integer $n$ , $f ^ { k } ( n ) = 1$ for some positiveinteger $k$ , where
$$
f (n) = \left\{ \begin{array}{l l} n / 2, & \text {i f n i s e v e n ; a n d} \\ 3 n + 1 & \text {o t h e r w i s e ,} \end{array} \right.
$$
and $f ^ { k } ( n )$ means to apply the function $f k$ times: $f ^ { k } ( n ) = f ( f ( \cdot \cdot \cdot f ( n ) \cdot \cdot \cdot ) )$ .{z ?? times
For example, for $n = 5$ , the sequence is
$$
\begin{array}{c} 5   1 6   8   4   2   1, \end{array}
$$
and for $n = 7$ , the sequence is
$$
7 2 2 1 1 3 4 1 7 5 2 2 6 1 3 4 0 2 0 1 0 5 1 6 8 4 2 1.
$$
The number of iterations it takes to reach 1 varies erratically. Here is a list of various $n$ andthe number of iterations required for that ??. These number are in hexadecimal, which youwill eventually display on the DE1-SoC.
<table><tr><td>7</td><td>11</td><td>17</td><td>10</td><td>27</td><td>23</td><td>f7</td><td>30</td><td>3ff</td><td>3f</td><td>40f</td><td>3f</td><td>4ef</td><td>b1</td></tr><tr><td>8</td><td>4</td><td>18</td><td>b</td><td>28</td><td>9</td><td>f8</td><td>6e</td><td>400</td><td>b</td><td>410</td><td>20</td><td>4f0</td><td>28</td></tr><tr><td>9</td><td>14</td><td>19</td><td>18</td><td>29</td><td>6e</td><td>f9</td><td>30</td><td>401</td><td>25</td><td>411</td><td>7d</td><td>4f1</td><td>28</td></tr><tr><td>a</td><td>7</td><td>1a</td><td>b</td><td>2a</td><td>9</td><td>fa</td><td>6e</td><td>402</td><td>25</td><td>412</td><td>7d</td><td>4f2</td><td>20</td></tr><tr><td>b</td><td>f</td><td>1b</td><td>70</td><td>2b</td><td>1e</td><td>fb</td><td>42</td><td>403</td><td>25</td><td>413</td><td>7d</td><td>4f3</td><td>20</td></tr><tr><td>c</td><td>a</td><td>1c</td><td>13</td><td>2c</td><td>11</td><td>fc</td><td>6e</td><td>404</td><td>7d</td><td>414</td><td>20</td><td>4f4</td><td>28</td></tr><tr><td>d</td><td>a</td><td>1d</td><td>13</td><td>2d</td><td>11</td><td>fd</td><td>6e</td><td>405</td><td>7d</td><td>415</td><td>20</td><td>4f5</td><td>28</td></tr><tr><td>e</td><td>12</td><td>1e</td><td>13</td><td>2e</td><td>11</td><td>fe</td><td>30</td><td>406</td><td>7d</td><td>416</td><td>7d</td><td>4f6</td><td>20</td></tr><tr><td>f</td><td>12</td><td>1f</td><td>6b</td><td>2f</td><td>69</td><td>ff</td><td>30</td><td>407</td><td>25</td><td>417</td><td>7d</td><td>4f7</td><td>20</td></tr><tr><td>10</td><td>5</td><td>20</td><td>6</td><td>30</td><td>c</td><td>100</td><td>9</td><td>408</td><td>7d</td><td>418</td><td>20</td><td>4f8</td><td>3a</td></tr><tr><td>11</td><td>d</td><td>21</td><td>1b</td><td>31</td><td>19</td><td>101</td><td>7b</td><td>409</td><td>9c</td><td>419</td><td>3f</td><td>4f9</td><td>20</td></tr><tr><td>12</td><td>15</td><td>22</td><td>e</td><td>32</td><td>19</td><td>102</td><td>7b</td><td>40a</td><td>7d</td><td>41a</td><td>20</td><td>4fa</td><td>3a</td></tr><tr><td>13</td><td>15</td><td>23</td><td>e</td><td>33</td><td>19</td><td>103</td><td>7b</td><td>40b</td><td>7d</td><td>41b</td><td>5e</td><td>4fb</td><td>54</td></tr><tr><td>14</td><td>8</td><td>24</td><td>16</td><td>34</td><td>c</td><td>104</td><td>1e</td><td>40c</td><td>7d</td><td>41c</td><td>51</td><td>4fc</td><td>3a</td></tr><tr><td>15</td><td>8</td><td>25</td><td>16</td><td>35</td><td>c</td><td>105</td><td>1e</td><td>40d</td><td>7d</td><td>41d</td><td>51</td><td>4fd</td><td>3a</td></tr><tr><td>16</td><td>10</td><td>26</td><td>16</td><td>36</td><td>71</td><td>106</td><td>1e</td><td>40e</td><td>3f</td><td>41e</td><td>51</td><td>4fe</td><td>85</td></tr></table>
# 4 Write and Test a Collatz Sequence Generator
Implement a module that can test the Collatz conjecture for a particular $n$ by completingthe body of the provided collatz module in collatz.sv. Its interface is
module collatz( input logic clk, //Clock input logic go, //Load value from n; start iterating input logic [31:0] n, //Startvalue;only read when  $\mathrm{go} = 1$  output logic [31:0] dout, //Iteration value:true after  $\mathrm{go} = 1$  output logic done); //Truewhendoutreaches1
In every cycle, if go is true, the module should reset and start counting from the 32-bitunsigned integer value on n. The dout signal should always output the current value. Inevery other clock cycle, the module should compute the next number in the sequence bychecking whether the current number is positive and either divide by two or multiply bythree and add one. When the value reaches 1, done should be asserted and the moduleshould stop producing new numbers until the next go input.
Here is the correct output running with the input 7. Note that dout is loaded with 7 startingat the first rising edge of the clock where go is asserted, but 22, the second number in thesequence, only appears in the first cycle after go is asserted.
clk   
go   
n[31:0] 7   
dout[31:0] 0 7 22 11 34 17 52 26 13 40 20 10 5 16 8 4 2 1 done
We have provided a Verilator test bench that supplies appropriate inputs to the collatzmodule (i.e., clk, go, and n). To compile your collatz.sv file with the provided test benchinto a Verilator simulator, run make obj_dir/Vcollatz or
verilator -trace -Wall -cc collatz.sv -exe collatz.cpp -top-module collatz  
cd obj_dir  
make -j -f Vcollatz.mk
Run the Verilator simulator for this module by typing make collatz.vcd or./obj_dir/Vcollatz, which prints the sequence it finds. When working, it prints
$ ./obj_dir/Vcollatz
7 22 11 34 17 52 26 13 40 20 10 5 16 8 4 2 1
As a side-effect, running the Vcollatz simulation produces a Value Change Dump (VCD)file collatz.vcd, which you can view with the gtkwave program. Invoke it with gtkwavecollatz.vcd. A “save” file controls which signals are displayed. You may use the oneprovided by running gtkwave --save=collatz.gtkw collatz.vcd.
# 5 Write and Test a Module That Checks a Range of Values
Write a module that uses your Collatz sequence generator to test the Collatz hypothesisfor a sequence of numbers and record the number of iterations each took. Fill in the bodyof the range module provided in range.sv. Its interface is
module range  
# (parameter  
RAMWORDS = 16, // Number of counts to store in RAM  
RAM_ADDR BITS = 4) // Number of RAM address bits  
(input logic clk, // Clock  
input logic go, // Read start and start testing  
input logic [31:0] start, // Number to start from or count to read  
output logic done, // True once memory is filled  
output logic [15:0] count); // Iteration count data once finished
The two compile-time parameters RAM_WORDS and RAM_ADDR_BITS set the size of thememory in which the iteration counts should be stored.
The go signal should tell the module to read the start input and start generating Collatziterations from that number. The number of iterations it takes to reach 1 starting from startshould be written into address 0 in ram; the number of iterations from start $^ { + 1 }$ should bewritten into address 1, etc. Finally, done should be asserted when the ram is filled.
Once done is asserted, applying an address to start should read the memory from thataddress and present it on count in the next cycle.
Fill in the skeleton range.sv file provided. While you may modify anything you want except the interface to the module, we suggest you use the ram and internal signals provided.
We have supplied a testbench file range.cpp that provides the clock, go, and start signals,then waits for done before reading out the number of iterations observed by applyingdifferent values of start to read the value out through the count signal.
As before, make range.vcd will compile the simulator, run the testbench, print the iterationcounts that are written to memory, and write the range.vcd file.
Below is the timing diagram of our solution as it starts; your solution only has to obey the protocol at the interface (clk, go, start, done, and count).
![image](/652294a0436a4f6378ddc735fda9c6c5be20c042e7cdaf1354fb266d73ddfddf.jpg)

The go input switches running to true; loads n with the value on start; resets num, the ramaddress, to 0; sets din, the iteration count, to 1; and pulses cgo high for a cycle to start theCollatz iterator module.
Every running cycle when cgo is low before cdone goes high, din, the number that will beultimately written to the ram, is increased by one. When the Collatz module asserts cdone,the we signal is pulsed high for just a single cycle to write the current count (din) to theram at the address in num. The cycle after cdone is asserted, $n$ is increased by one, din isreset to 1, and cgo is asserted again to start testing the next value.
7 17  
8 4  
9 20  
10 7  
11 15  
12 10  
13 10  
14 18  
15 18  
16 5  
17 13  
18 21  
19 21  
20 8  
21 8  
22 16
Below is the timing diagram of our solution as it finishes the Collatz iterations and switchesto reading out the numbers it found.
![image](/2992497aabaf3259899b2c05f4b880cc52d3d9b898b3c84af350298d89fcf893.jpg)

After we is asserted for the highest ram address (15, set by RAM_WORDS), running turns offand done pulses once. This tells the testbench to feed 0, 1, 2, . . . into start and the number ofiterations for each value is read out on count. For example, start $= 0$ produces a 17 on count,corresponding to $n = 7$ , and start $= 1 1$ produces a 21, corresponding to $n = 7 + 1 1 = 1 8$ .
Running the Vrange simulator directly prints out these iteration counts:
$ ./obj_dir/Vrange
Note that these counts match those listed earlier, although they were in hexadecimal.
# 6 Set the fpga Configuration Mode
A microscopic set ofswitches on the back ofthe board controls thesource of the fpga’s con-figuration information.For this lab, set it to the“Active Serial” mode asshown on the right.
![image](/627ae8c1942ae64249071d07f02f26ac1bb0509c9196e8c678c102f0537bee12.jpg)

![image](/b29e3d25a85f181a94c0a5494699a92c62f765d43eed2b16d1d194940aa6d01d.jpg)

For this lab, we will be configuring the fpga with a jtag interface through usb; the ActiveSerial mode makes the board start up in a factory demonstration mode.
<table><tr><td>Mode</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td></tr><tr><td>Active Serial (Default; use for this lab)</td><td>Off</td><td>Off</td><td>On</td><td>On</td><td>Off</td><td>On</td></tr><tr><td>FPPx16 (from SD card; later labs will use this)</td><td>Off</td><td>On</td><td>On</td><td>On</td><td>On</td><td>On</td></tr><tr><td>FPPx32 (from Linux)</td><td>Off</td><td>On</td><td>Off</td><td>On</td><td>Off</td><td>On</td></tr></table>
# 7 Compile and Download the Project Via the Command-Line
Enter the lab1 directory and run make lab1.qpf to create the project from de1-soc-project.tcl.This should report “Info (23030): Evaluation of Tcl script de1-soc-project.tcl was successful,”but if it complains “quartus_sh: Command not found,” make sure your PATH variableincludes the directory for the Quartus binaries (on the 1235 Mudd machines, this is doneby /etc/profile.d/quartus.sh when you log in). Running this script creates lab1.qpf (the mainproject file), lab1.qsf (settings, including files and pins), and lab1.sdc (clock constraints).
Compile the project from the command line with make output_files/lab1.sof. This readsthe .sv files and ultimately produces the lab1.sof (sram object) file, which is downloaded tothe fpga to run your project. This takes a while, and will report and handful of warnings, butshould eventually report “Info (293000): Quartus Prime Full Compilation was successful.”You may ignore warnings “(292013) Feature LogicLock” and “(15714) Some pins haveincomplete I/O assignments”; others should be fixed.
Connect the DE1-SoC board to your workstation. Connect the$+ 1 2 \mathrm { V }$ power supply to the board near the red power button, con-nect a usb cable to the “usb Blaster” port on the board (next to thepower button) and to your workstation, and power on the boardwith the red button.
![image](/ada678c3b3b9bdff0199b4cd81ff036d51f76058905006a74f83d81b4396bf8d.jpg)

Once your project is compiled, download the .sof file to the DE1-SoC board by runningmake program. A variety of things can go wrong. If you get “Error (213013): Programminghardware cable not detected,” check your board’s power and usb connection to yourworkstation.
When powered and connected, the board should appear as a usb device. Under Linux,running lsusb should report the board as 09fb:6810 Altera or 09fb:6010 Altera.
The Quartus software must also have permission to access the port. Run jtagconfig. Withthe board connected and powered on, it should report
$ jtagconfig
1) DE-SoC [1-1.5.2.2]
4BA00477 SOCVHPS
02D120DD 5CSE(BA5|MA5)/5CSTFD5D5/..
If lsusb “sees” the board but jtagconfig reports “No JTAG hardware available,” there is apermission problem, which can be resolved by telling udev to make the board accessible toeverybody. As root, create the file /etc/udev/rules.d/51-altera.rules containing
ATTR{idVendor $\scriptstyle 3 = = ^ { \prime \prime } 0 9 + 6 ^ { \prime \prime }$ , ATTR{idProduct} $\} = = " 6 0 1 0 "$ , MODE="0666"
ATTR{idVendor $\scriptstyle 3 = = ^ { \prime \prime } 0 9 + 6 ^ { \prime \prime }$ , ATTR{idProduct} $\} = = " 6 8 1 0 "$ , MODE $\mathop { \left. \sum \right.} $ "0666"
# 8 Compile and Download the Project Via the GUI (optional)
The project can also be compiled and downloaded via the Quartus gui. Start from adirectory with a clean unpack of lab1.tar.gz (or run make clean), then start Quartus bytyping quartus.
Create the project files by running a Tcl script. Open the Tcl console window withView Utility Windows. . . →Tcl Console. Type source de1-soc-project.tcl in the Quar-tus Prime Tcl Console window. This will create the project files lab1.qpf, lab1.qsf, andlab1.sdc.
Open the lab1.qpf project with File Open Project. . . .
Compile the project with Processing Start Compilation. This will take a while and shouldeventually report “Quartus Prime Full Compilation was successful.” There should be noerrors, but there may be warnings.
Download the configuration to the fpga. Select Tools Programmer. If “No Hardware”appears, connect the board to your workstations via usb and power it on (see the previoussection), then click on “Hardware Setup. . . ” You should see “DE-SoC” under “Availablehardware items.” Select “De-SoC[· · · ]” under “Currently selected hardware” and click“Close.”
Set up the jtag chain byclicking on “Auto Detect”and select “5CSEMA5.”Answer “yes” if it asksto update the program-mer’s device list.
Tell it to configure thefpga with the lab1.soffile by clicking on the“5CSEMA5” device then“Change File” and choosethe lab1.sof file in theoutput_files directory.Mark the “Program/-Configure” checkbox onthe 5CSEMA5F31 line.It should look like theimage on the right.
![image](/a4584f3eff77c00f0224ee6145a6786b55a79b946244fbbf5ebca7a13ac342b6.jpg)

Finally, click on “Start” to program the fpga. This should quickly report $^ { \circ } 1 0 0 \%$ (Successful)”on the programmer.
# 9 Add a User Interface
Add a user interface that uses the four pushbuttons and the ten switches to test the numberof iterations taken to reach 1 for various values of ??. Modify the lab1.sv file we provided.Have the ten switches sw[9:0] control the value $n$ at the start of the range to test. Makethe leftmost button key[3] run the range module over 256 values (i.e., trigger go) startingfrom the value on the switches (in binary).
Use your hex7seg module to make the leftmost three seven-segment displays show thevalue of $n$ (specifically, the lower twelve bits) and have the rightmost three displays showthe number of iterations taken to reach 1 for that value of $n$ .
For example, if you enter 7 (in binary) on the switches and press key[3], the display shouldshow 007011, which indicates $n = 7$ takes 17 iterations (in decimal).
Make it so that the rightmost buttons, key[0] and key[1] increment and decrement thevalue of $n$ being displayed. Make it so holding them makes the value change about 5 times asecond, e.g., by using a 22-bit counter running off the 50 MHz clock and only changing thevalue when this counter wraps around. The lowest $n$ should always be set by the switches;the buttons should just control which number (between $n$ and $n + 2 5 5$ ) is being read out.
Make it so key[2] (second to left) resets the difference between the $n$ displayed and thevalue on the switches.

Also, there should be an indicator when calculation is finished after pressing key[3]. Now, it need to flash all LED once, from LEDR[9] to LEDR[0] to indicate that calculation is finished

At last, make sure there is no misregirtration issue. The testbench requres pressing one button from either key[0] or key[1] rapidly, expected result is the incrementation and decrementation from screen should reflect result in real time. when pressing one of the key very rapidly, all input is registered.

# Demonstrate Your Working System
Every team needs to demonstrate their lab 1 design to a ta. The main objective of the demois to test the user interface, so it will focus on the buttons, switches, and seven-segmentdisplays. We will check the Collatz values from your submitted code.
You can demonstrate your working system during ta office hours.
We will check the following input and output during the demo:
• switch input
• seven-segment display output
• button input
– regular button press
– fast button press
– slow button press
– button press and hold
– multiple button press
• indication that range is complete
This rubric deliberately does not specify exactly what your system should do in each ofthese cases because we want you to think about what “the right thing” is according to whata person would expect. For many of these actions there are multiple appropriate responses.Consider what you would expect from each action, and design your system accordingly.The tas are happy to discuss what is “reasonable” behavior if you have questions.

- Here is a sample rubic:

|  |  |  |
|----|----|----|
| Action | Check | Observed |
| Set SW\[9:0\] to 00000011Press KEY\[3\]seven-segment display shows 007011 | there is an indication that range is completeseven-segment display correct | Correct but no indication range calculation is completed |
| Press KEY\[0\] 5 times at a normal pace seven-segment display shows 00C00A | each keypress increments nno missed keypresses or jumps | Missing Some Presses |
| Press KEY\[1\] 3 times at a normal pace seven-segment display shows 009014 | each keypress decrements nno missed keypresses or jumps | Missing Some Presses |
| Press KEY\[2\]seven-segment display shows 007011 | value of n resets to switch valueseven-segment display correct | Correct |
| Press KEY\[0\] 5 times fast seven-segment display shows 00C00A | each keypress increments nno missed keypresses or jumps | Missing Some Presses |
| Press KEY\[0\] 5 times slowly seven-segment display shows 01100D | each keypress increments nno missed keypresses or jumps | Correct |
| Press and hold KEY\[1\] a bit longer than it takes for the hex display to reach the edge of range | display decrements at a human-readable pacen never goes below the switch valuecan either stop at 007 or wrap to 106 and keep decreasing | Stops at end of range, decrement too fast |
| Press KEY\[0\] and KEY\[1\] at the same time | behave as if one button pressed, ordo nothing, orflicker once | Always acts as Key\[0\] |
| Set SW\[9:0\] to 1100000111Press KEY\[3\]seven-segment display shows 307099 | there is an indication that range is completeseven-segment display correct | Correct but no indication range is complete. |