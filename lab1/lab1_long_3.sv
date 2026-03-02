// CSEE 4840 Lab 1: Run and Display Collatz Conjecture Iteration Counts
//
// Spring 2026
//
// By: Hao Cai , Chenhao Yang
// Uni: hc3612 , cy2822
//
// Version without press-and-hold auto-repeat (simple flat 5 Hz repeat)

module lab1( input logic        CLOCK_50,  // 50 MHz Clock input

	     input logic [3:0] 	KEY, // Pushbuttons; KEY[0] is rightmost

	     input logic [9:0] 	SW, // Switches; SW[0] is rightmost

	     // 7-segment LED displays; HEX0 is rightmost
	     output logic [6:0] HEX0, HEX1, HEX2, HEX3, HEX4, HEX5,

	     output logic [9:0] LEDR // LEDs above the switches; LED[0] on right
	     );

   logic 			clk;
   assign clk = CLOCK_50;

   // ---- Synchronize active-low KEY inputs (2-stage synchronizer) ----
   logic [3:0] key_s1, key_s2;
   always_ff @(posedge clk) begin
      key_s1 <= KEY;
      key_s2 <= key_s1;
   end

   // ---- Cooldown-based edge filter (80ms) ----
   // Accept first transition IMMEDIATELY, then lock out 80 ms.
   // Unlike stability-debounce, this never misses short taps.
   localparam [22:0] COOLDOWN = 23'd3_999_999;  // 80 ms at 50 MHz
   logic [3:0]  key_clean = 4'b1111;            // filtered state (init: released)
   logic [22:0] cd0 = 23'd0, cd1 = 23'd0, cd2 = 23'd0, cd3 = 23'd0;

   always_ff @(posedge clk) begin
      // KEY[0]
      if (cd0 > 23'd0)                              cd0 <= cd0 - 23'd1;
      else if (key_s2[0] != key_clean[0]) begin      cd0 <= COOLDOWN;
                                                     key_clean[0] <= key_s2[0]; end
      // KEY[1]
      if (cd1 > 23'd0)                              cd1 <= cd1 - 23'd1;
      else if (key_s2[1] != key_clean[1]) begin      cd1 <= COOLDOWN;
                                                     key_clean[1] <= key_s2[1]; end
      // KEY[2]
      if (cd2 > 23'd0)                              cd2 <= cd2 - 23'd1;
      else if (key_s2[2] != key_clean[2]) begin      cd2 <= COOLDOWN;
                                                     key_clean[2] <= key_s2[2]; end
      // KEY[3]
      if (cd3 > 23'd0)                              cd3 <= cd3 - 23'd1;
      else if (key_s2[3] != key_clean[3]) begin      cd3 <= COOLDOWN;
                                                     key_clean[3] <= key_s2[3]; end
   end

   // Falling edge on filtered signal = confirmed press
   logic [3:0] key_clean_d;
   always_ff @(posedge clk) begin
      key_clean_d <= key_clean;
   end

   logic [3:0] key_fell;
   assign key_fell = key_clean_d & ~key_clean;

   // ---- Range module signals ----
   logic        go, done;
   logic [31:0] start;
   logic [15:0] count;

   // ---- State registers ----
   logic [9:0] base_n  = 10'd0;  // Captured SW value when KEY[3] pressed
   logic [7:0] offset  = 8'd0;   // Offset from base (0..255)

   // ---- Go pulse on KEY[3] press ----
   logic go_pulse;
   assign go_pulse = key_fell[3];
   assign go = go_pulse;

   // ---- Start mux: SW value during go, offset for reading ----
   assign start = go_pulse ? {22'd0, SW[9:0]} : {24'd0, offset};

   // ---- Range module (256 entries, 8-bit address) ----
   range #(256, 8) r (.clk(clk),
                       .go(go),
                       .start(start),
                       .done(done),
                       .count(count));

   // ---- Auto-repeat counter for KEY[0] / KEY[1] ----
   // 500 ms initial delay, then 5 Hz (200 ms) repeat
   logic [24:0] rep_cnt      = 25'd0;
   logic        initial_wait = 1'b1;
   logic        rep_tick;

   always_ff @(posedge clk) begin
      if (key_s2[0] & key_s2[1]) begin        // neither button held (raw)
         rep_cnt      <= 25'd0;
         initial_wait <= 1'b1;
      end else if (initial_wait) begin         // first hold: 500 ms delay
         if (rep_cnt == 25'd24_999_999) begin
            rep_cnt      <= 25'd0;
            initial_wait <= 1'b0;
         end else
            rep_cnt <= rep_cnt + 25'd1;
      end else begin                           // subsequent: 200 ms repeat
         if (rep_cnt == 25'd9_999_999)
            rep_cnt <= 25'd0;
         else
            rep_cnt <= rep_cnt + 25'd1;
      end
   end
   assign rep_tick = ( initial_wait && rep_cnt == 25'd24_999_999) |
                     (!initial_wait && rep_cnt == 25'd9_999_999);

   // ---- Increment / decrement actions (KEY[0] has priority) ----
   logic inc, dec;
   assign inc = key_fell[0] | (~key_clean[0] & rep_tick);
   assign dec = (key_fell[1] | (~key_clean[1] & rep_tick)) & ~inc;

   // ---- Base and offset management ----
   always_ff @(posedge clk) begin
      if (go_pulse) begin
         base_n <= SW[9:0];
         offset <= 8'd0;
      end else begin
         if (key_fell[2])
            offset <= 8'd0;
         else if (inc && offset < 8'd255)
            offset <= offset + 8'd1;
         else if (dec && offset > 8'd0)
            offset <= offset - 8'd1;
      end
   end

   // ---- Display: n on HEX5-3, count on HEX2-0 ----
   logic [11:0] n_disp;
   assign n_disp = {2'b0, base_n} + {4'b0, offset};

   hex7seg h5(.a(n_disp[11:8]), .y(HEX5));
   hex7seg h4(.a(n_disp[7:4]),  .y(HEX4));
   hex7seg h3(.a(n_disp[3:0]),  .y(HEX3));

   hex7seg h2(.a(count[11:8]),  .y(HEX2));
   hex7seg h1(.a(count[7:4]),   .y(HEX1));
   hex7seg h0(.a(count[3:0]),   .y(HEX0));

   // ---- LED sweep on completion: LEDR[9] → LEDR[0] ----
   logic [3:0]  led_pos = 4'd0;
   logic [22:0] led_tmr = 23'd0;
   logic        led_on  = 1'b0;

   always_ff @(posedge clk) begin
      if (done) begin
         led_on  <= 1'b1;
         led_pos <= 4'd0;
         led_tmr <= 23'd0;
      end else if (go_pulse) begin
         led_on  <= 1'b0;        // stop sweep on new computation
      end else if (led_on) begin
         if (led_tmr == 23'd4_999_999) begin  // 0.1s per LED position
            led_tmr <= 23'd0;
            if (led_pos == 4'd9)
               led_on <= 1'b0;   // sweep finished
            else
               led_pos <= led_pos + 4'd1;
         end else
            led_tmr <= led_tmr + 23'd1;
      end
   end

   always_comb begin
      LEDR = 10'd0;
      if (led_on)
         LEDR = 10'(10'd1 << (4'd9 - led_pos));
   end

endmodule
