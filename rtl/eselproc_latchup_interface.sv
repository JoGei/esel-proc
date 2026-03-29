module sync_fifo #(
  parameter int unsigned WIDTH = 32,
  parameter int unsigned DEPTH = 16
)(
  input  logic                   clk,
  input  logic                   reset,
  input  logic                   clr,

  input  logic                   push,
  input  logic [WIDTH-1:0]       push_data,

  input  logic                   pop,

  output logic [WIDTH-1:0]       peek_data,
  output logic                   empty,
  output logic                   full,
  output logic [$clog2(DEPTH+1)-1:0] level
);

  localparam int unsigned PTR_W = (DEPTH <= 1) ? 1 : $clog2(DEPTH);

  logic [WIDTH-1:0] mem [0:DEPTH-1];
  logic [PTR_W-1:0] wr_ptr_q, rd_ptr_q;
  logic [$clog2(DEPTH+1)-1:0] count_q;

  integer i;

  assign empty     = (count_q == 0);
  assign full      = (count_q == DEPTH);
  assign level     = count_q;
  assign peek_data = mem[rd_ptr_q];

  always_ff @(posedge clk) begin
    if (reset) begin
      wr_ptr_q <= '0;
      rd_ptr_q <= '0;
      count_q  <= '0;
      for (i = 0; i < DEPTH; i = i + 1) begin
        mem[i] <= '0;
      end
    end else if (clr) begin
      wr_ptr_q <= '0;
      rd_ptr_q <= '0;
      count_q  <= '0;
    end else begin
      if (push && !full) begin
        mem[wr_ptr_q] <= push_data;
        wr_ptr_q <= (wr_ptr_q == DEPTH-1) ? '0 : (wr_ptr_q + 1'b1);
      end

      if (pop && !empty) begin
        rd_ptr_q <= (rd_ptr_q == DEPTH-1) ? '0 : (rd_ptr_q + 1'b1);
      end

      unique case ({push && !full, pop && !empty})
        2'b10: count_q <= count_q + 1'b1;
        2'b01: count_q <= count_q - 1'b1;
        default: count_q <= count_q;
      endcase
    end
  end

endmodule
module eselproc_latchup_interface #(
  parameter logic [31:0] BASE_ADDR         = 32'h0002_0000,
  parameter int unsigned N_IN_W_PAYLOADS   = 2,
  parameter int unsigned N_OUT_W_PAYLOADS  = 1,
  parameter int unsigned INPUT_FIFO_DEPTH  = 16,
  parameter int unsigned OUTPUT_FIFO_DEPTH = 16
)(
  input  logic clk,
  input  logic reset,

  output logic                              o_ready,
  input  logic                              i_valid,
  input  logic [N_IN_W_PAYLOADS-1:0][31:0]  i_payload,

  output logic [N_OUT_W_PAYLOADS-1:0][31:0] o_payload,
  output logic                              o_valid,

  input  logic                              i_wb_cyc,
  input  logic                              i_wb_stb,
  input  logic                              i_wb_we,
  input  logic [3:0]                        i_wb_sel,
  input  logic [31:0]                       i_wb_adr,
  input  logic [31:0]                       i_wb_dat,
  output logic [31:0]                       o_wb_dat,
  output logic                              o_wb_ack
);

  initial begin
    if (N_IN_W_PAYLOADS < 1)  $error("N_IN_W_PAYLOADS must be >= 1");
    if (N_OUT_W_PAYLOADS < 1) $error("N_OUT_W_PAYLOADS must be >= 1");
    if (INPUT_FIFO_DEPTH < 1)  $error("INPUT_FIFO_DEPTH must be >= 1");
    if (OUTPUT_FIFO_DEPTH < 1) $error("OUTPUT_FIFO_DEPTH must be >= 1");
  end

  localparam int unsigned IN_FIFO_W  = 32 * N_IN_W_PAYLOADS;
  localparam int unsigned OUT_FIFO_W = 32 * N_OUT_W_PAYLOADS;

  //--------------------------------------------------------------------------
  // Register map
  //--------------------------------------------------------------------------
  localparam logic [7:0] STATUS_REG_OFFSET    = 8'h00;
  localparam logic [7:0] CONTROL_REG_OFFSET   = 8'h04;
  localparam logic [7:0] IN_POP_REG_OFFSET    = 8'h08;
  localparam logic [7:0] IN_DATA_BASE_OFFSET  = 8'h0C;
  localparam logic [7:0] OUT_DATA_BASE_OFFSET = 8'h40;
  localparam logic [7:0] OUT_PUSH_REG_OFFSET  = 8'h80;

  // CONTROL register
  // [15:0]  sample_count_q
  // [16]    flush_q        (software sets, HW clears once flushing starts)
  // [17]    clr_in_fifo    (pulse, HW auto-clears)
  // [18]    clr_out_fifo   (pulse, HW auto-clears)

  // STATUS register
  // [0]     in_empty
  // [1]     in_full
  // [2]     out_empty
  // [3]     out_full
  // [4]     flushing
  // [5]     sample_count_zero
  // [15:8]  in_level
  // [23:16] out_level

  logic [31:0] control_q, control_n;
  logic [31:0] status_q,  status_n;
  logic [31:0] wb_rdata_q, wb_rdata_n;

  logic [IN_FIFO_W-1:0]  in_shadow_q, in_shadow_n;
  logic [OUT_FIFO_W-1:0] out_stage_q, out_stage_n;

  logic                  flushing_q, flushing_n;
  logic                  o_valid_q,  o_valid_n;
  logic [OUT_FIFO_W-1:0] o_payload_q, o_payload_n;

  logic wb_req, wb_fire, wb_req_d;
  logic [7:0] wb_addr_off;

  logic [IN_FIFO_W-1:0]  in_fifo_push_data;
  logic [IN_FIFO_W-1:0]  in_fifo_peek_data;
  logic                  in_fifo_push;
  logic                  in_fifo_pop;
  logic                  in_fifo_empty;
  logic                  in_fifo_full;
  logic [$clog2(INPUT_FIFO_DEPTH+1)-1:0] in_fifo_level;

  logic [OUT_FIFO_W-1:0] out_fifo_push_data;
  logic [OUT_FIFO_W-1:0] out_fifo_peek_data;
  logic                  out_fifo_push;
  logic                  out_fifo_pop;
  logic                  out_fifo_empty;
  logic                  out_fifo_full;
  logic [$clog2(OUTPUT_FIFO_DEPTH+1)-1:0] out_fifo_level;

  logic [15:0] sample_count_q;

  integer k;

  function automatic logic [31:0] apply_wstrb(
    input logic [31:0] old_val,
    input logic [31:0] new_val,
    input logic [3:0]  sel
  );
    logic [31:0] tmp;
    begin
      tmp = old_val;
      if (sel[0]) tmp[7:0]   = new_val[7:0];
      if (sel[1]) tmp[15:8]  = new_val[15:8];
      if (sel[2]) tmp[23:16] = new_val[23:16];
      if (sel[3]) tmp[31:24] = new_val[31:24];
      apply_wstrb = tmp;
    end
  endfunction

  //--------------------------------------------------------------------------
  // Wishbone request decode
  //--------------------------------------------------------------------------
  assign wb_req      = i_wb_cyc && i_wb_stb &&
                       (i_wb_adr[31:10] == BASE_ADDR[31:10]);
  assign wb_fire     = wb_req && !wb_req_d;
  assign wb_addr_off = i_wb_adr[7:0];

  always_ff @(posedge clk) begin
    if (reset)
      wb_req_d <= 1'b0;
    else
      wb_req_d <= wb_fire;
  end

  assign o_wb_ack = wb_req_d;
  assign o_wb_dat = wb_rdata_q;

  assign sample_count_q = control_q[15:0];

  //--------------------------------------------------------------------------
  // Input vector pack
  //--------------------------------------------------------------------------
  always_comb begin
    for (k = 0; k < N_IN_W_PAYLOADS; k = k + 1) begin
      in_fifo_push_data[32*k +: 32] = i_payload[k];
    end
  end

  //--------------------------------------------------------------------------
  // Output vector unpack
  //--------------------------------------------------------------------------
  always_comb begin
    for (k = 0; k < N_OUT_W_PAYLOADS; k = k + 1) begin
      o_payload[k] = o_payload_q[32*k +: 32];
    end
  end

  //--------------------------------------------------------------------------
  // Derived handshake
  //--------------------------------------------------------------------------
  assign o_ready      = (sample_count_q != 16'd0) && !in_fifo_full;
  assign in_fifo_push = i_valid && o_ready;

  //--------------------------------------------------------------------------
  // FIFOs
  //--------------------------------------------------------------------------
  sync_fifo #(
    .WIDTH(IN_FIFO_W),
    .DEPTH(INPUT_FIFO_DEPTH)
  ) u_in_fifo (
    .clk      (clk),
    .reset    (reset),
    .clr      (control_q[17]),
    .push     (in_fifo_push),
    .push_data(in_fifo_push_data),
    .pop      (in_fifo_pop),
    .peek_data(in_fifo_peek_data),
    .empty    (in_fifo_empty),
    .full     (in_fifo_full),
    .level    (in_fifo_level)
  );

  sync_fifo #(
    .WIDTH(OUT_FIFO_W),
    .DEPTH(OUTPUT_FIFO_DEPTH)
  ) u_out_fifo (
    .clk      (clk),
    .reset    (reset),
    .clr      (control_q[18]),
    .push     (out_fifo_push),
    .push_data(out_fifo_push_data),
    .pop      (out_fifo_pop),
    .peek_data(out_fifo_peek_data),
    .empty    (out_fifo_empty),
    .full     (out_fifo_full),
    .level    (out_fifo_level)
  );

  //--------------------------------------------------------------------------
  // Main next-state logic
  //--------------------------------------------------------------------------
  always_comb begin : comb_main
    control_n    = control_q;
    status_n     = 32'h0000_0000;
    wb_rdata_n   = wb_rdata_q;
    in_shadow_n  = in_shadow_q;
    out_stage_n  = out_stage_q;
    flushing_n   = flushing_q;
    o_valid_n    = 1'b0;
    o_payload_n  = o_payload_q;

    in_fifo_pop        = 1'b0;
    out_fifo_push      = 1'b0;
    out_fifo_pop       = 1'b0;
    out_fifo_push_data = out_stage_q;

    // Accepted input vector decrements sample_count
    if (in_fifo_push) begin
      control_n[15:0] = control_q[15:0] - 16'd1;
    end

    // Auto-clear pulse bits
    if (control_q[17]) begin
      control_n[17] = 1'b0;
    end
    if (control_q[18]) begin
      control_n[18] = 1'b0;
      flushing_n    = 1'b0;
      o_valid_n     = 1'b0;
    end

    //----------------------------------------------------------------------
    // MMIO writes
    //----------------------------------------------------------------------
    if (wb_fire && i_wb_we) begin
      unique case (wb_addr_off)
        CONTROL_REG_OFFSET: begin
          control_n = apply_wstrb(control_q, i_wb_dat, i_wb_sel);
        end

        OUT_PUSH_REG_OFFSET: begin
          if (!out_fifo_full) begin
            out_fifo_push = 1'b1;
          end
        end

        default: begin
          if ((wb_addr_off >= OUT_DATA_BASE_OFFSET) &&
              (wb_addr_off < (OUT_DATA_BASE_OFFSET + 4*N_OUT_W_PAYLOADS)) &&
              (wb_addr_off[1:0] == 2'b00)) begin
            integer idx;
            logic [31:0] word_old;
            logic [31:0] word_new;
            idx = (wb_addr_off - OUT_DATA_BASE_OFFSET) >> 2;
            word_old = out_stage_q[32*idx +: 32];
            word_new = apply_wstrb(word_old, i_wb_dat, i_wb_sel);
            out_stage_n[32*idx +: 32] = word_new;
          end
        end
      endcase
    end

    //----------------------------------------------------------------------
    // MMIO reads
    //----------------------------------------------------------------------
    if (wb_fire && !i_wb_we) begin
      unique case (wb_addr_off)
        STATUS_REG_OFFSET: begin
          wb_rdata_n = status_q;
        end

        CONTROL_REG_OFFSET: begin
          wb_rdata_n = control_q;
        end

        IN_POP_REG_OFFSET: begin
          if (!in_fifo_empty) begin
            in_shadow_n = in_fifo_peek_data;
            in_fifo_pop = 1'b1;
            wb_rdata_n  = 32'h0000_0001;
          end else begin
            wb_rdata_n  = 32'h0000_0000;
          end
        end

        default: begin
          if ((wb_addr_off >= IN_DATA_BASE_OFFSET) &&
              (wb_addr_off < (IN_DATA_BASE_OFFSET + 4*N_IN_W_PAYLOADS)) &&
              (wb_addr_off[1:0] == 2'b00)) begin
            integer idx;
            idx = (wb_addr_off - IN_DATA_BASE_OFFSET) >> 2;
            wb_rdata_n = in_shadow_q[32*idx +: 32];
          end else if ((wb_addr_off >= OUT_DATA_BASE_OFFSET) &&
                        (wb_addr_off < (OUT_DATA_BASE_OFFSET + 4*N_OUT_W_PAYLOADS)) &&
                        (wb_addr_off[1:0] == 2'b00)) begin
            integer idx;
            idx = (wb_addr_off - OUT_DATA_BASE_OFFSET) >> 2;
            wb_rdata_n = out_stage_q[32*idx +: 32];
          end else begin
            wb_rdata_n = 32'h0000_0000;
          end
        end
      endcase
    end

    //----------------------------------------------------------------------
    // Flush control
    //----------------------------------------------------------------------
    if (!flushing_q && control_q[16] && !out_fifo_empty) begin
      flushing_n    = 1'b1;
      // control_n[16] = 1'b0; auto-flush i.e. don't clear flush request after flush
    end

    // While flushing, emit one vector per cycle.
    // Capture payload into o_payload_n before popping FIFO.
    if (flushing_q) begin
      if (!out_fifo_empty) begin
        o_payload_n = out_fifo_peek_data;
        o_valid_n   = 1'b1;
        out_fifo_pop = 1'b1;

        if (out_fifo_level == 1) begin
          flushing_n = 1'b0;
        end
      end else begin
        o_valid_n  = 1'b0;
        flushing_n = 1'b0;
      end
    end

    //----------------------------------------------------------------------
    // Status
    //----------------------------------------------------------------------
    status_n[0]      = in_fifo_empty;
    status_n[1]      = in_fifo_full;
    status_n[2]      = out_fifo_empty;
    status_n[3]      = out_fifo_full;
    status_n[4]      = flushing_n;
    status_n[5]      = (control_n[15:0] == 16'd0);
    status_n[15:8]   = in_fifo_level;
    status_n[23:16]  = out_fifo_level;
  end

  //--------------------------------------------------------------------------
  // Sequential state update
  //--------------------------------------------------------------------------
  always_ff @(posedge clk) begin
    if (reset) begin
      control_q   <= 32'h0000_0000;
      status_q    <= 32'h0000_0000;
      wb_rdata_q  <= 32'h0000_0000;
      in_shadow_q <= '0;
      out_stage_q <= '0;
      flushing_q  <= 1'b0;
      o_valid_q   <= 1'b0;
      o_payload_q <= '0;
    end else begin
      control_q   <= control_n;
      status_q    <= status_n;
      wb_rdata_q  <= wb_rdata_n;
      in_shadow_q <= in_shadow_n;
      out_stage_q <= out_stage_n;
      flushing_q  <= flushing_n;
      o_valid_q   <= o_valid_n;
      o_payload_q <= o_payload_n;
    end
  end

  assign o_valid = o_valid_q;

endmodule