module collatz( input logic         clk,   // Clock
		input logic 	    go,    // Load value from n; start iterating
		input logic  [31:0] n,     // Start value; only read when go = 1
		output logic [31:0] dout,  // Iteration value: true after go = 1
		output logic 	    done); // True when dout reaches 1

   /* Replace this comment and the code below with your solution */

    always_ff @(posedge clk) begin
        if (go) begin
            // Restart from n
            dout <= n;

            // Stop if n is already 0 or 1, otherwise start iterating
            done <= (n <= 32'd1);
        end

        else if (!done) begin
            // Only advance if not finished
            logic [31:0] next;

            //Check even or odd
            if (dout[0] == 1'b0)
                next = dout >> 1;              // even: n/2
            else
                next = (dout * 32'd3) + 32'd1; // odd: 3n + 1

            // Update dout and done
            dout <= next;
            done <= (next == 32'd1);
        end
        // else: done stays asserted and dout holds its value (should be 1)
    end

   /* make collatz.vcd (based on tb) */
   /* Pass (7 22 11 34 17 52 26 13 40 20 10 5 16 8 4 2 1 for n = 7) */

endmodule
