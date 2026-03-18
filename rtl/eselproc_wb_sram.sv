module eselproc_wb_sram #(
  parameter logic [31:0] BASE_ADDR = 32'h0001_0000,
  parameter int MEM_WORDS = 256
)(
  input  logic        clk,
  input  logic        reset,
  input  logic        i_wb_cyc,
  input  logic        i_wb_stb,
  input  logic        i_wb_we,
  input  logic [3:0]  i_wb_sel,
  input  logic [31:0] i_wb_adr,
  input  logic [31:0] i_wb_dat,
  output logic [31:0] o_wb_dat,
  output logic        o_wb_ack
);

  logic [31:0] mem [0:MEM_WORDS-1];
  logic        req_q;
  logic [31:0] rdata_q;

  logic [31:0] word_addr_full;
  logic [$clog2(MEM_WORDS)-1:0] word_idx;

  wire sel = i_wb_cyc && i_wb_stb &&
             (i_wb_adr >= BASE_ADDR) &&
             (i_wb_adr < (BASE_ADDR + MEM_WORDS*4));

  assign word_addr_full = (i_wb_adr - BASE_ADDR) >> 2;
  assign word_idx       = word_addr_full[$clog2(MEM_WORDS)-1:0];

  always_ff @(posedge clk) begin
    if (reset) begin
      req_q   <= 1'b0;
      rdata_q <= 32'h0;
    end else begin
      req_q <= sel && !req_q;

      if (sel && !req_q) begin
        if (i_wb_we) begin
          if (i_wb_sel[0]) mem[word_idx][7:0]   <= i_wb_dat[7:0];
          if (i_wb_sel[1]) mem[word_idx][15:8]  <= i_wb_dat[15:8];
          if (i_wb_sel[2]) mem[word_idx][23:16] <= i_wb_dat[23:16];
          if (i_wb_sel[3]) mem[word_idx][31:24] <= i_wb_dat[31:24];
        end
        rdata_q <= mem[word_idx];
      end
    end
  end

  assign o_wb_ack = req_q;
  assign o_wb_dat = rdata_q;

endmodule