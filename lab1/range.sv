module range
   #(parameter
     RAM_WORDS = 16,            // Number of counts to store in RAM
     RAM_ADDR_BITS = 4)         // Number of RAM address bits
   (input logic         clk,    // Clock
    input logic 	go,           // Read start and start testing
    input logic [31:0] 	start,  // Number to start from or count to read
    output logic 	done,         // True once memory is filled
    output logic [15:0] count); // Iteration count once finished

   logic 		cgo;             // go for the Collatz iterator
   logic                cdone;  // done from the Collatz iterator
   logic [31:0] 	n;            // number to start the Collatz iterator

// verilator lint_off PINCONNECTEMPTY
   
   // Instantiate the Collatz iterator
   collatz c1(.clk(clk),
	      .go(cgo),
	      .n(n),
	      .done(cdone),
	      .dout());

   logic [RAM_ADDR_BITS - 1:0] 	 num;         // The RAM address to write
   logic 			 running = 0; // True during the iterations

   /* Replace this comment and the code below with your solution,
      which should generate running, done, cgo, n, num, we, and din */
   
   // Extra stuff
   logic start_next = 1'b0;  // to start the next cycle
   logic wrote      = 1'b0;  // prevents repeated writes while cdone stays high
   logic [31:0] n_reg;

   // The last address we will write to (one less than the number of words), not sure about it
   localparam logic [RAM_ADDR_BITS-1:0] LAST_ADDR = RAM_ADDR_BITS'(RAM_WORDS - 1);

   // Start a new full sweep only when idle
   logic start_now;
   assign start_now = go && !running;

   // Pulse Collatz for one cycle at the start, and once per subsequent value
   assign cgo = start_now || start_next;

   // Make sure Collatz sees the correct n on the same edge
   assign n   = start_now ? start : n_reg;

   // go high for one cycle when cdone first appears
   assign we  = running && cdone && !wrote;

   always_ff @(posedge clk) begin
      // done is a pulse
      done <= 1'b0;

      // Start the first Collatz run when we see the go signal, and set up for subsequent runs
      if (start_now) begin
         running    <= 1'b1;
         n_reg      <= start;
         num        <= '0;
         if (start <= 32'd1)
            din        <= 16'd0;
         else
            din        <= 16'd1;
         wrote      <= 1'b0;
         start_next <= 1'b0;

      // During a run, we want to write the count when cdone goes high, 
      // and then set up the next run on the following cycle
      end else if (running) begin

         // Clear the cycle restart pulse after it has been used
         if (start_next) begin
            start_next <= 1'b0;
            wrote      <= 1'b0; // ready to detect the next completion
         end

         if (we) begin
            // A write will occur on this clock edge
            wrote <= 1'b1;

            // If we've just written to the last address, we're done. 
            // Otherwise, set up for the next Collatz run.
            if (num == LAST_ADDR) begin
               running <= 1'b0;
               done    <= 1'b1; // pulse done once RAM is filled
            end else begin
               logic [31:0] next_n;
               next_n    = n_reg + 32'd1;
               num        <= num + 1'b1;
               n_reg      <= next_n;
               if (next_n <= 32'd1)
                  din        <= 16'd0;
               else
                  din        <= 16'd1;
               start_next <= 1'b1; // start next Collatz run in the next cycle
            end

         // Ifnot writing, but Collatz just finished, set up for the next run
         end else if (!cdone && !start_next) begin
            // Count how many terms we've seen (starts at 1)
            din <= din + 16'd1;
         end
      end
   end

   /* Replace this comment and the code above with your solution */

   logic 			 we;                    // Write din to addr
   logic [15:0] 		 din;                   // Data to write
   logic [15:0] 		 mem[RAM_WORDS - 1:0];  // The RAM itself
   logic [RAM_ADDR_BITS - 1:0] 	 addr;                  // Address to read/write

   assign addr = we ? num : start[RAM_ADDR_BITS-1:0];
   
   always_ff @(posedge clk) begin
      if (we) mem[addr] <= din;
      count <= mem[addr];      
   end

endmodule

/* make range.vcd

obj_dir/Vrange
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

*/

