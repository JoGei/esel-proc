module eselproc_wb_dbus_interconnect #(
  parameter logic [31:0] ROM_BASE        = 32'h0000_0000,
  parameter logic [31:0] ROM_SIZE        = 32'h0001_0000,
  parameter logic [31:0] SRAM_BASE       = 32'h0001_0000,
  parameter logic [31:0] SRAM_SIZE       = 32'h0000_0400,
  parameter logic [31:0] LATCHUP_IF_BASE = 32'h0002_0000,
  parameter logic [31:0] LATCHUP_IF_SIZE = 32'h0000_0400
)(
  input  logic        clk,
  input  logic        reset,

  // Wishbone master side (from SERV dbus)
  input  logic        i_wb_cyc,
  input  logic        i_wb_stb,
  input  logic        i_wb_we,
  input  logic [3:0]  i_wb_sel,
  input  logic [31:0] i_wb_adr,
  input  logic [31:0] i_wb_dat,
  output logic [31:0] o_wb_dat,
  output logic        o_wb_ack,

  // Wishbone slave side: ROM
  output logic        o_rom_wb_cyc,
  output logic        o_rom_wb_stb,
  output logic        o_rom_wb_we,
  output logic [3:0]  o_rom_wb_sel,
  output logic [31:0] o_rom_wb_adr,
  output logic [31:0] o_rom_wb_dat,
  input  logic [31:0] i_rom_wb_dat,
  input  logic        i_rom_wb_ack,

  // Wishbone slave side: SRAM
  output logic        o_sram_wb_cyc,
  output logic        o_sram_wb_stb,
  output logic        o_sram_wb_we,
  output logic [3:0]  o_sram_wb_sel,
  output logic [31:0] o_sram_wb_adr,
  output logic [31:0] o_sram_wb_dat,
  input  logic [31:0] i_sram_wb_dat,
  input  logic        i_sram_wb_ack,

  // Wishbone slave side: latchup interface
  output logic        o_lif_wb_cyc,
  output logic        o_lif_wb_stb,
  output logic        o_lif_wb_we,
  output logic [3:0]  o_lif_wb_sel,
  output logic [31:0] o_lif_wb_adr,
  output logic [31:0] o_lif_wb_dat,
  input  logic [31:0] i_lif_wb_dat,
  input  logic        i_lif_wb_ack
);

  localparam logic [31:0] ROM_END        = ROM_BASE + ROM_SIZE - 1;
  localparam logic [31:0] SRAM_END       = SRAM_BASE + SRAM_SIZE - 1;
  localparam logic [31:0] LATCHUP_IF_END = LATCHUP_IF_BASE + LATCHUP_IF_SIZE - 1;

  logic sel_rom;
  logic sel_sram;
  logic sel_lif;
  logic sel_none;

  // Optional: remember invalid accesses so they still get an ACK
  logic invalid_req_q;

  assign sel_rom  = i_wb_cyc && i_wb_stb &&
                    (i_wb_adr >= ROM_BASE) &&
                    (i_wb_adr <= ROM_END);

  assign sel_sram = i_wb_cyc && i_wb_stb &&
                    (i_wb_adr >= SRAM_BASE) &&
                    (i_wb_adr <= SRAM_END);

  assign sel_lif  = i_wb_cyc && i_wb_stb &&
                    (i_wb_adr >= LATCHUP_IF_BASE) &&
                    (i_wb_adr <= LATCHUP_IF_END);

  assign sel_none = i_wb_cyc && i_wb_stb && !sel_rom && !sel_sram && !sel_lif;

  always_ff @(posedge clk) begin
    if (reset)
      invalid_req_q <= 1'b0;
    else
      invalid_req_q <= sel_none;
  end

  // Route master request only to the selected slave
  assign o_rom_wb_cyc = sel_rom;
  assign o_rom_wb_stb = sel_rom;
  assign o_rom_wb_we  = i_wb_we;
  assign o_rom_wb_sel = i_wb_sel;
  assign o_rom_wb_adr = i_wb_adr;
  assign o_rom_wb_dat = i_wb_dat;

  assign o_sram_wb_cyc = sel_sram;
  assign o_sram_wb_stb = sel_sram;
  assign o_sram_wb_we  = i_wb_we;
  assign o_sram_wb_sel = i_wb_sel;
  assign o_sram_wb_adr = i_wb_adr;
  assign o_sram_wb_dat = i_wb_dat;

  assign o_lif_wb_cyc = sel_lif;
  assign o_lif_wb_stb = sel_lif;
  assign o_lif_wb_we  = i_wb_we;
  assign o_lif_wb_sel = i_wb_sel;
  assign o_lif_wb_adr = i_wb_adr;
  assign o_lif_wb_dat = i_wb_dat;

  // Return path mux
  always_comb begin
    o_wb_dat = 32'h0000_0000;

    if (i_rom_wb_ack)
      o_wb_dat = i_rom_wb_dat;
    else if (i_sram_wb_ack)
      o_wb_dat = i_sram_wb_dat;
    else if (i_lif_wb_ack)
      o_wb_dat = i_lif_wb_dat;
    else if (invalid_req_q)
      o_wb_dat = 32'h0000_0000;
  end

  // ACK comes from the selected slave, or from invalid access handler
  assign o_wb_ack = i_rom_wb_ack | i_sram_wb_ack | i_lif_wb_ack | invalid_req_q;

endmodule