module hex7seg(input logic  [3:0] a,
	       output logic [6:0] y);

/* Replace this comment and the code below it with your solution */
   
   always_comb begin
      case (a)
         //                  gfe_dcba
         4'h0: y = 7'h40; // 100_0000 (0)
         4'h1: y = 7'h79; // 111_1001 (1)
         4'h2: y = 7'h24; // 010_0100 (2)
         4'h3: y = 7'h30; // 011_0000 (3)
         4'h4: y = 7'h19; // 001_1001 (4)
         4'h5: y = 7'h12; // 001_0010 (5)
         4'h6: y = 7'h02; // 000_0010 (6)
         4'h7: y = 7'h78; // 111_1000 (7)
         4'h8: y = 7'h00; // 000_0000 (8)
         4'h9: y = 7'h10; // 001_0000 (9)
         4'hA: y = 7'h08; // 000_1000 (A)
         4'hB: y = 7'h03; // 000_0011 (b)
         4'hC: y = 7'h46; // 100_0110 (C)
         4'hD: y = 7'h21; // 010_0001 (d)
         4'hE: y = 7'h06; // 000_0110 (E)
         4'hF: y = 7'h0E; // 000_1110 (F)
         default: y = 7'h7F; // All off
      endcase
   end

   /* make hex7seg */
   /* All passed */
   
endmodule
