/*
 * Avalon memory-mapped peripheral that generates VGA
 *
 * Stephen A. Edwards
 * Columbia University
 *
 * Register map (16-bit word addressed):
 *
 * Word Addr  Byte Offset  Bits[15:0]   Meaning
 *     0          0         ball_x       Ball center X coordinate (0-639)
 *     1          2         ball_y       Ball center Y coordinate (0-479)
 *
 * Ball is rendered as a filled white circle of radius RADIUS pixels
 * on a dark blue background. Shadow registers latch to active
 * registers at vertical blanking to prevent tearing.
 */

module vga_ball(input logic         clk,
                input logic         reset,
                input logic [15:0]  writedata,
                input logic         write,
                input logic         chipselect,
                input logic         address,

                output logic [7:0]  VGA_R, VGA_G, VGA_B,
                output logic        VGA_CLK, VGA_HS, VGA_VS,
                                    VGA_BLANK_n,
                output logic        VGA_SYNC_n);

   parameter RADIUS = 16;
   localparam RADIUS_SQ = RADIUS * RADIUS;

   logic [10:0] hcount;
   logic [9:0]  vcount;

   /* Shadow registers (written by Avalon bus) */
   logic [9:0] ball_x_shadow, ball_y_shadow;

   /* Active registers (used by display logic, latched at vsync) */
   logic [9:0] ball_x, ball_y;

   vga_counters counters(.clk50(clk), .*);

   /* Avalon write: update shadow registers */
   always_ff @(posedge clk)
     if (reset) begin
        ball_x_shadow <= 10'd320;
        ball_y_shadow <= 10'd240;
     end else if (chipselect && write)
       case (address)
         1'b0 : ball_x_shadow <= writedata[9:0];
         1'b1 : ball_y_shadow <= writedata[9:0];
       endcase

   /* Vsync latch: copy shadow to active at start of vertical blanking */
   always_ff @(posedge clk)
     if (reset) begin
        ball_x <= 10'd320;
        ball_y <= 10'd240;
     end else if (vcount == 10'd480 && hcount == 11'd0) begin
        ball_x <= ball_x_shadow;
        ball_y <= ball_y_shadow;
     end

   /* Ball rendering: circle equation (px-cx)^2 + (py-cy)^2 <= r^2 */
   wire [9:0] pixel_x = hcount[10:1];
   wire [9:0] pixel_y = vcount[9:0];

   wire signed [10:0] dx = {1'b0, pixel_x} - {1'b0, ball_x};
   wire signed [10:0] dy = {1'b0, pixel_y} - {1'b0, ball_y};

   wire [21:0] dist_sq = dx * dx + dy * dy;

   always_comb begin
      {VGA_R, VGA_G, VGA_B} = {8'h0, 8'h0, 8'h0};
      if (VGA_BLANK_n) begin
        if (dist_sq <= RADIUS_SQ)
          {VGA_R, VGA_G, VGA_B} = {8'hff, 8'hff, 8'hff};
        else
          {VGA_R, VGA_G, VGA_B} = {8'h0, 8'h0, 8'h80};
      end
   end

endmodule

module vga_counters(
 input logic         clk50, reset,
 output logic [10:0] hcount,  // hcount[10:1] is pixel column
 output logic [9:0]  vcount,  // vcount[9:0] is pixel row
 output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n);

/*
 * 640 X 480 VGA timing for a 50 MHz clock: one pixel every other cycle
 * 
 * HCOUNT 1599 0             1279       1599 0
 *             _______________              ________
 * ___________|    Video      |____________|  Video
 * 
 * 
 * |SYNC| BP |<-- HACTIVE -->|FP|SYNC| BP |<-- HACTIVE
 *       _______________________      _____________
 * |____|       VGA_HS          |____|
 */
   // Parameters for hcount
   parameter HACTIVE      = 11'd 1280,
             HFRONT_PORCH = 11'd 32,
             HSYNC        = 11'd 192,
             HBACK_PORCH  = 11'd 96,   
             HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC +
                            HBACK_PORCH; // 1600
   
   // Parameters for vcount
   parameter VACTIVE      = 10'd 480,
             VFRONT_PORCH = 10'd 10,
             VSYNC        = 10'd 2,
             VBACK_PORCH  = 10'd 33,
             VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC +
                            VBACK_PORCH; // 525

   logic endOfLine;
   
   always_ff @(posedge clk50 or posedge reset)
     if (reset)          hcount <= 0;
     else if (endOfLine) hcount <= 0;
     else                hcount <= hcount + 11'd 1;

   assign endOfLine = hcount == HTOTAL - 1;
       
   logic endOfField;
   
   always_ff @(posedge clk50 or posedge reset)
     if (reset)          vcount <= 0;
     else if (endOfLine)
       if (endOfField)   vcount <= 0;
       else              vcount <= vcount + 10'd 1;

   assign endOfField = vcount == VTOTAL - 1;

   // Horizontal sync: from 0x520 to 0x5DF (0x57F)
   // 101 0010 0000 to 101 1101 1111
   assign VGA_HS = !( (hcount[10:8] == 3'b101) &
                      !(hcount[7:5] == 3'b111));
   assign VGA_VS = !( vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);

   assign VGA_SYNC_n = 1'b0; // For putting sync on the green signal; unused
   
   // Horizontal active: 0 to 1279     Vertical active: 0 to 479
   // 101 0000 0000  1280              01 1110 0000  480
   // 110 0011 1111  1599              10 0000 1100  524
   assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
                        !( vcount[9] | (vcount[8:5] == 4'b1111) );

   /* VGA_CLK is 25 MHz
    *             __    __    __
    * clk50    __|  |__|  |__|
    *        
    *             _____       __
    * hcount[0]__|     |_____|
    */
   assign VGA_CLK = hcount[0]; // 25 MHz clock: rising edge sensitive
   
endmodule
