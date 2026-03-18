module eselproc_soc #(
  parameter logic [31:0] ROM_BASE        = 32'h0000_0000,
  parameter logic [31:0] ROM_SIZE        = 32'h0000_4000,
  parameter logic [31:0] RAM_BASE        = 32'h0001_0000,
  parameter logic [31:0] RAM_SIZE        = 32'h0000_0400,
  parameter logic [31:0] LATCHUP_IF_BASE = 32'h0002_0000,
  parameter logic [31:0] LATCHUP_IF_SIZE = 32'h0000_0400
)(
  input  logic        clk,
  input  logic        reset,

  // External Wishbone peripheral socket for latchup_interface
  output logic        o_lif_wb_cyc,
  output logic        o_lif_wb_stb,
  output logic        o_lif_wb_we,
  output logic [3:0]  o_lif_wb_sel,
  output logic [31:0] o_lif_wb_adr,
  output logic [31:0] o_lif_wb_dat,
  input  logic [31:0] i_lif_wb_dat,
  input  logic        i_lif_wb_ack
);

  // SERV instruction bus
  logic [31:0] ibus_adr;
  logic [31:0] ibus_rdata;
  logic        ibus_cyc;
  logic        ibus_stb;
  logic        ibus_ack;

  // SERV data bus
  logic [31:0] dbus_adr;
  logic [31:0] dbus_wdata;
  logic [31:0] dbus_rdata;
  logic [3:0]  dbus_sel;
  logic        dbus_we;
  logic        dbus_cyc;
  logic        dbus_stb;
  logic        dbus_ack;

  // Interconnect -> SRAM
  logic        sram_wb_cyc;
  logic        sram_wb_stb;
  logic        sram_wb_we;
  logic [3:0]  sram_wb_sel;
  logic [31:0] sram_wb_adr;
  logic [31:0] sram_wb_wdata;
  logic [31:0] sram_wb_rdata;
  logic        sram_wb_ack;

  // CPU
  assign ibus_stb = ibus_cyc;
  assign dbus_stb = dbus_cyc;
  serv_rf_top #(
    .RESET_PC(ROM_BASE),
    .COMPRESSED(0),
    .PRE_REGISTER(0)
  ) u_cpu (
    .clk        (clk),
    .i_rst      (reset),
    .i_timer_irq(1'b0),

    .o_ibus_adr (ibus_adr),   // ok
    .o_ibus_cyc (ibus_cyc),   // ok
    //.o_ibus_stb (ibus_stb), // ok: does not exist assign from cyc
    .i_ibus_rdt (ibus_rdata), // ok
    .i_ibus_ack (ibus_ack),   // ok

    .o_dbus_adr (dbus_adr),   // ok
    .o_dbus_dat (dbus_wdata), // ok
    .o_dbus_sel (dbus_sel),   // ok
    .o_dbus_we  (dbus_we),    // ok
    .o_dbus_cyc (dbus_cyc),   // ok
    //.o_dbus_stb (dbus_stb), // ok: does not exist assign from cyc
    .i_dbus_rdt (dbus_rdata), // ok
    .i_dbus_ack (dbus_ack),   // ok

    // Extension: unused as of now
    .o_ext_rs1(),
    .o_ext_rs2(),
    .o_ext_funct3(),
    .i_ext_rd('0),
    .i_ext_ready('0),
    // MDU
    .o_mdu_valid()
  );

  // ROM
  eselproc_wb_rom #(
    .BASE_ADDR (ROM_BASE),
    .MEM_WORDS (ROM_SIZE/4)
  ) u_rom (
    .clk      (clk),
    .reset    (reset),

    .i_wb_cyc (ibus_cyc),
    .i_wb_stb (ibus_stb),
    .i_wb_we  (1'b0),
    .i_wb_sel (4'b1111),
    .i_wb_adr (ibus_adr),
    .i_wb_dat (32'h0000_0000),
    .o_wb_dat (ibus_rdata),
    .o_wb_ack (ibus_ack)
  );

  // Data interconnect
  // SRAM is internal, latchup peripheral port is exported
  eselproc_wb_dbus_interconnect #(
    .SRAM_BASE       (RAM_BASE),
    .SRAM_SIZE       (RAM_SIZE),
    .LATCHUP_IF_BASE (LATCHUP_IF_BASE),
    .LATCHUP_IF_SIZE (LATCHUP_IF_SIZE)
  ) u_dbus_ic (
    .clk          (clk),
    .reset        (reset),

    .i_wb_cyc     (dbus_cyc),
    .i_wb_stb     (dbus_stb),
    .i_wb_we      (dbus_we),
    .i_wb_sel     (dbus_sel),
    .i_wb_adr     (dbus_adr),
    .i_wb_dat     (dbus_wdata),
    .o_wb_dat     (dbus_rdata),
    .o_wb_ack     (dbus_ack),

    .o_sram_wb_cyc(sram_wb_cyc),
    .o_sram_wb_stb(sram_wb_stb),
    .o_sram_wb_we (sram_wb_we),
    .o_sram_wb_sel(sram_wb_sel),
    .o_sram_wb_adr(sram_wb_adr),
    .o_sram_wb_dat(sram_wb_wdata),
    .i_sram_wb_dat(sram_wb_rdata),
    .i_sram_wb_ack(sram_wb_ack),

    .o_lif_wb_cyc (o_lif_wb_cyc),
    .o_lif_wb_stb (o_lif_wb_stb),
    .o_lif_wb_we  (o_lif_wb_we),
    .o_lif_wb_sel (o_lif_wb_sel),
    .o_lif_wb_adr (o_lif_wb_adr),
    .o_lif_wb_dat (o_lif_wb_dat),
    .i_lif_wb_dat (i_lif_wb_dat),
    .i_lif_wb_ack (i_lif_wb_ack)
  );

  // SRAM
  eselproc_wb_sram #(
    .BASE_ADDR (RAM_BASE),
    .MEM_WORDS (RAM_SIZE/4)
  ) u_sram (
    .clk      (clk),
    .reset    (reset),

    .i_wb_cyc (sram_wb_cyc),
    .i_wb_stb (sram_wb_stb),
    .i_wb_we  (sram_wb_we),
    .i_wb_sel (sram_wb_sel),
    .i_wb_adr (sram_wb_adr),
    .i_wb_dat (sram_wb_wdata),
    .o_wb_dat (sram_wb_rdata),
    .o_wb_ack (sram_wb_ack)
  );

endmodule