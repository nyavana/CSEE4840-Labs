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

   // ---- Debounce: require stable ~80ms before accepting change ----
   localparam [21:0] DB_THRESH = 22'd3_999_999;  // 80 ms at 50 MHz
   logic [3:0]  key_db = 4'b1111;                // debounced (init: all released)
   logic [21:0] db0 = 22'd0, db1 = 22'd0, db2 = 22'd0, db3 = 22'd0;

   always_ff @(posedge clk) begin
      // KEY[0]
      if (key_s2[0] == key_db[0])                                    db0 <= 22'd0;
      else if (db0 == DB_THRESH) begin db0 <= 22'd0; key_db[0] <= key_s2[0]; end
      else                                                           db0 <= db0 + 22'd1;
      // KEY[1]
      if (key_s2[1] == key_db[1])                                    db1 <= 22'd0;
      else if (db1 == DB_THRESH) begin db1 <= 22'd0; key_db[1] <= key_s2[1]; end
      else                                                           db1 <= db1 + 22'd1;
      // KEY[2]
      if (key_s2[2] == key_db[2])                                    db2 <= 22'd0;
      else if (db2 == DB_THRESH) begin db2 <= 22'd0; key_db[2] <= key_s2[2]; end
      else                                                           db2 <= db2 + 22'd1;
      // KEY[3]
      if (key_s2[3] == key_db[3])                                    db3 <= 22'd0;
      else if (db3 == DB_THRESH) begin db3 <= 22'd0; key_db[3] <= key_s2[3]; end
      else                                                           db3 <= db3 + 22'd1;
   end

   // Falling edge on debounced signal = confirmed button press
   logic [3:0] key_prev;
   always_ff @(posedge clk) begin
      key_prev <= key_db;
   end

   logic [3:0] key_fell;
   assign key_fell = key_prev & ~key_db;

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
   // 50 MHz / 10,000,000 = 5 Hz repeat rate (flat, no initial delay)
   logic [23:0] rep_cnt = 24'd0;
   logic        rep_tick;

   always_ff @(posedge clk) begin
      if (key_db[0] & key_db[1])          // neither button held
         rep_cnt <= 24'd0;
      else if (rep_cnt == 24'd9_999_999)   // wrap at 5 Hz
         rep_cnt <= 24'd0;
      else
         rep_cnt <= rep_cnt + 24'd1;
   end
   assign rep_tick = (rep_cnt == 24'd9_999_999);

   // ---- Increment / decrement actions (KEY[0] has priority) ----
   logic inc, dec;
   assign inc = key_fell[0] | (~key_db[0] & rep_tick);
   assign dec = (key_fell[1] | (~key_db[1] & rep_tick)) & ~inc;

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
