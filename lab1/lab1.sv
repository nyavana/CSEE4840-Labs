// CSEE 4840 Lab 1: Run and Display Collatz Conjecture Iteration Counts
module lab1(
    input  logic        CLOCK_50,   // 50 MHz Clock input
    input  logic [3:0]   KEY,        // Pushbuttons; KEY[0] is rightmost
    input  logic [9:0]   SW,         // Switches; SW[0] is rightmost
    output logic [6:0]   HEX0, HEX1, HEX2, HEX3, HEX4, HEX5,
    output logic [9:0]   LEDR
);

  logic clk;
  assign clk  = CLOCK_50;
  // LED sweep animation state for completion indicator
  logic        sweep_active = 1'b0;  // True during sweep animation
  logic [3:0]  sweep_pos    = 4'd9;  // Current LED position (9 down to 0)
  logic [21:0] sweep_timer  = 22'd0; // Timer for sweep speed (~80ms per step)

  // DE1-SoC pushbuttons are typically active-low (pressed = 0)
  logic btn_inc, btn_dec, btn_rst, btn_run;
  assign btn_inc = ~KEY[0]; // rightmost
  assign btn_dec = ~KEY[1];
  assign btn_rst = ~KEY[2];
  assign btn_run = ~KEY[3]; // leftmost

  // Range module interface
  logic        go;
  logic        done;
  logic [31:0] start;
  logic [15:0] count;

  range #(256, 8) r (
      .clk   (clk),
      .go    (go),
      .start (start),
      .done  (done),
      .count (count)
  );

  // Offset selects which result (0..255) we read out
  logic [7:0]  offset = 8'd0;

  // Slow tick: ~3 Hz for comfortable auto-repeat using a 24-bit counter
  logic [23:0] slow_ctr = 24'd0;
  logic        tick;
  logic        tick_prev = 1'b0;
  // Detect 0->1 transition of bit 23 for ~3 Hz repeat rate
  assign tick = slow_ctr[23] && !tick_prev;

  // Button debouncing
  localparam logic [19:0] DEBOUNCE_CYCLES = 20'd1_000_000; // 20ms at 50 MHz
  logic [19:0] inc_debounce = 20'd0;
  logic [19:0] dec_debounce = 20'd0;
  logic        inc_stable = 1'b0;
  logic        dec_stable = 1'b0;
  logic        inc_stable_d = 1'b0;
  logic        dec_stable_d = 1'b0;

  // Hold detection for inc/dec: short press = single step, long press = repeat
  localparam logic [25:0] HOLD_CYCLES = 26'd50_000_000; // ~1 s at 50 MHz
  logic [25:0] inc_hold_ctr = 26'd0;
  logic [25:0] dec_hold_ctr = 26'd0;
  logic        inc_held = 1'b0;
  logic        dec_held = 1'b0;

  // Press-edge detection for inc/dec buttons
  logic inc_press, dec_press;
  assign inc_press = inc_stable && !inc_stable_d;
  assign dec_press = dec_stable && !dec_stable_d;

  // Track when the memory has been filled (since done is a pulse)
  logic filled  = 1'b0;
  logic filling = 1'b0;
  logic [9:0] last_sw = 10'd0;

  // Detect a press edge on KEY[3]
  logic run_d = 1'b0;
  always_ff @(posedge clk) begin
    run_d <= btn_run;
  end
  wire run_edge = btn_run & ~run_d;

  // Make a 2-cycle go pulse on run_edge
  logic [1:0] go_sh = 2'b00;

  always_ff @(posedge clk) begin
    // default shift-down
    go_sh <= {1'b0, go_sh[1]};

    // free-running counters
    slow_ctr <= slow_ctr + 24'd1;
    tick_prev <= slow_ctr[23];

    // Button debouncing: require button to be stable for DEBOUNCE_CYCLES
    if (btn_inc) begin
      if (inc_debounce < DEBOUNCE_CYCLES)
        inc_debounce <= inc_debounce + 20'd1;
      else
        inc_stable <= 1'b1;
    end else begin
      inc_debounce <= 20'd0;
      inc_stable <= 1'b0;
    end

    if (btn_dec) begin
      if (dec_debounce < DEBOUNCE_CYCLES)
        dec_debounce <= dec_debounce + 20'd1;
      else
        dec_stable <= 1'b1;
    end else begin
      dec_debounce <= 20'd0;
      dec_stable <= 1'b0;
    end

    // Track stable signal edges for single-press detection
    inc_stable_d <= inc_stable;
    dec_stable_d <= dec_stable;

    // If switches change, force a re-run (prevents mismatched base vs stored RAM)
    // Also resets offset back to 0.
    if (SW != last_sw) begin
      offset  <= 8'd0;
      filled  <= 1'b0;
      filling <= 1'b0;
      go_sh   <= 2'b00;
      inc_hold_ctr <= 26'd0;
      dec_hold_ctr <= 26'd0;
      inc_held <= 1'b0;
      dec_held <= 1'b0;
      inc_debounce <= 20'd0;
      dec_debounce <= 20'd0;
      inc_stable <= 1'b0;
      dec_stable <= 1'b0;
    end

    // KEY[2] resets displayed n back to the switch value (offset = 0)
    if (btn_rst) begin
      offset <= 8'd0;
      inc_hold_ctr <= 26'd0;
      dec_hold_ctr <= 26'd0;
      inc_held <= 1'b0;
      dec_held <= 1'b0;
    end

    // Start a new run when KEY[3] is pressed (ignore if already filling)
    if (run_edge && !filling) begin
      offset   <= 8'd0;
      filled   <= 1'b0;
      filling  <= 1'b1;
      go_sh    <= 2'b11;   // 2-cycle pulse
      slow_ctr <= 24'd0;   // optional: makes button repeat timing feel consistent
      inc_hold_ctr <= 26'd0;
      dec_hold_ctr <= 26'd0;
      inc_held <= 1'b0;
      dec_held <= 1'b0;
      sweep_active <= 1'b0; // cancel any ongoing sweep
    end

    // Latch "filled" when range finishes; start LED sweep
    if (done) begin
      filling      <= 1'b0;
      filled       <= 1'b1;
      sweep_active <= 1'b1;
      sweep_pos    <= 4'd9;
      sweep_timer  <= 22'd0;
    end

    // LED sweep animation: advance one LED per ~80ms
    if (sweep_active) begin
      if (sweep_timer == 22'd3_999_999) begin // 4M cycles = 80ms at 50 MHz
        sweep_timer <= 22'd0;
        if (sweep_pos == 4'd0)
          sweep_active <= 1'b0;  // sweep finished
        else
          sweep_pos <= sweep_pos - 4'd1;
      end else begin
        sweep_timer <= sweep_timer + 22'd1;
      end
    end

    // Button handling: only active when filled
    if (filled) begin
      // Single press on rising edge (prevents both buttons acting simultaneously)
      if (inc_press && !dec_stable && offset != 8'hFF) begin
        offset <= offset + 8'd1;
      end else if (dec_press && !inc_stable && offset != 8'h00) begin
        offset <= offset - 8'd1;
      end

      // Hold detection: track how long button has been stable-high
      if (inc_stable) begin
        if (inc_hold_ctr >= HOLD_CYCLES)
          inc_held <= 1'b1;
        else
          inc_hold_ctr <= inc_hold_ctr + 26'd1;
      end else begin
        inc_hold_ctr <= 26'd0;
        inc_held <= 1'b0;
      end

      if (dec_stable) begin
        if (dec_hold_ctr >= HOLD_CYCLES)
          dec_held <= 1'b1;
        else
          dec_hold_ctr <= dec_hold_ctr + 26'd1;
      end else begin
        dec_hold_ctr <= 26'd0;
        dec_held <= 1'b0;
      end

      // Auto-repeat on slow tick when held
      if (tick) begin
        if (inc_held && !dec_stable && offset != 8'hFF)
          offset <= offset + 8'd1;
        else if (dec_held && !inc_stable && offset != 8'h00)
          offset <= offset - 8'd1;
      end
    end else begin
      // Reset hold state when not filled
      inc_hold_ctr <= 26'd0;
      dec_hold_ctr <= 26'd0;
      inc_held <= 1'b0;
      dec_held <= 1'b0;
    end

    // Track the last switch value for change detection.
    last_sw <= SW;
  end

  assign go = go_sh[0];

  // start mux:
  // - while filling (or not yet filled): feed base n from switches
  // - after filled: feed RAM address (offset) to read out count next cycle
  always_comb begin
    if (filled && !filling)
      start = {24'd0, offset};      // read address
    else
      start = {22'd0, SW};          // starting n
  end

  // Displayed n is SW + offset; show lower 12 bits
  logic [31:0] n_full;
  logic [11:0] n_disp;
  assign n_full = {22'd0, SW} + {24'd0, offset};
  assign n_disp = n_full[11:0];

  // Only show count once filled (optional; you can remove this if you prefer)
  logic [11:0] c_disp;
  assign c_disp = filled ? count[11:0] : 12'h000;

  // Six hex digits: [HEX5 HEX4 HEX3] [HEX2 HEX1 HEX0] = [n] [count]
  hex7seg h0(.a(c_disp[3:0]),   .y(HEX0));
  hex7seg h1(.a(c_disp[7:4]),   .y(HEX1));
  hex7seg h2(.a(c_disp[11:8]),  .y(HEX2));

  hex7seg h3(.a(n_disp[3:0]),   .y(HEX3));
  hex7seg h4(.a(n_disp[7:4]),   .y(HEX4));
  hex7seg h5(.a(n_disp[11:8]),  .y(HEX5));

  // LED output: sweep animation on completion, otherwise show switches
  always_comb begin
    if (sweep_active)
      LEDR = 10'd1 << sweep_pos;  // single LED lit, moving 9->0
    else
      LEDR = SW;                  // normal: show switch values
  end

endmodule
