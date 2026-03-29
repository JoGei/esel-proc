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

  // SERV data bus
  logic [31:0] dbus_adr;
  logic [31:0] dbus_wdata;
  logic [31:0] dbus_rdata;
  logic [3:0]  dbus_sel;
  logic        dbus_we;
  logic        dbus_cyc;
  logic        dbus_stb;
  logic        dbus_ack;

  // Interconnect -> ROM
  logic        rom_wb_cyc;
  logic        rom_wb_stb;
  logic        rom_wb_we;
  logic [3:0]  rom_wb_sel;
  logic [31:0] rom_wb_adr;
  logic [31:0] rom_wb_wdata;
  logic [31:0] rom_wb_rdata;
  logic        rom_wb_ack;

  // Interconnect -> SRAM
  logic        sram_wb_cyc;
  logic        sram_wb_stb;
  logic        sram_wb_we;
  logic [3:0]  sram_wb_sel;
  logic [31:0] sram_wb_adr;
  logic [31:0] sram_wb_wdata;
  logic [31:0] sram_wb_rdata;
  logic        sram_wb_ack;

  picorv32_wb #(
    .ENABLE_COUNTERS(0),
    .ENABLE_COUNTERS64(0),
    .ENABLE_REGS_16_31(1),
    .ENABLE_REGS_DUALPORT(1),
    .TWO_STAGE_SHIFT(1),
    .BARREL_SHIFTER(0),
    .TWO_CYCLE_COMPARE(0),
    .TWO_CYCLE_ALU(0),
    .COMPRESSED_ISA(0),
    .CATCH_MISALIGN(0),
    .CATCH_ILLINSN(0),
    .ENABLE_PCPI(0),
    .ENABLE_MUL(0),
    .ENABLE_FAST_MUL(0),
    .ENABLE_DIV(0),
    .ENABLE_IRQ(0),
    .ENABLE_IRQ_QREGS(1),
    .ENABLE_IRQ_TIMER(1),
    .ENABLE_TRACE(0),
    .REGS_INIT_ZERO(0),
    .MASKED_IRQ(32'h 0000_0000),
    .LATCHED_IRQ(32'h ffff_ffff),
    .PROGADDR_RESET(ROM_BASE),
    .PROGADDR_IRQ(32'h0000_0010),
    .STACKADDR(RAM_BASE + RAM_SIZE - 4)
  ) u_cpu(
    .trap         (),

    // Wishbone interfaces
    .wb_rst_i     (reset),
    .wb_clk_i     (clk),

    .wbm_adr_o    (dbus_adr),
    .wbm_dat_o    (dbus_wdata),
    .wbm_dat_i    (dbus_rdata),
    .wbm_we_o     (dbus_we),
    .wbm_sel_o    (dbus_sel),
    .wbm_stb_o    (dbus_stb),
    .wbm_ack_i    (dbus_ack),
    .wbm_cyc_o    (dbus_cyc),

    // Pico Co-Processor Interface (PCPI)
    .pcpi_valid   (),
    .pcpi_insn    (),
    .pcpi_rs1     (),
    .pcpi_rs2     (),
    .pcpi_wr      ('0),
    .pcpi_rd      ('0),
    .pcpi_wait    ('0),
    .pcpi_ready   ('0),

    // IRQ interface
    .irq          ('0),
    .eoi          (),

    // Trace Interface
    .trace_valid  (),
    .trace_data   (),

    .mem_instr    () //no idea what this is.
  );

  // ROM
  eselproc_wb_rom #(
    .BASE_ADDR (ROM_BASE),
    .MEM_WORDS (ROM_SIZE/4)
  ) u_rom (
    .clk      (clk),
    .reset    (reset),

    .i_wb_cyc (rom_wb_cyc),
    .i_wb_stb (rom_wb_stb),
    .i_wb_we  (rom_wb_we /* hopefully always low*/),
    .i_wb_sel (rom_wb_sel),
    .i_wb_adr (rom_wb_adr),
    .i_wb_dat (rom_wb_wdata /* hopefully not used*/),
    .o_wb_dat (rom_wb_rdata),
    .o_wb_ack (rom_wb_ack)
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

    .o_rom_wb_cyc (rom_wb_cyc),
    .o_rom_wb_stb (rom_wb_stb),
    .o_rom_wb_we  (rom_wb_we),
    .o_rom_wb_sel (rom_wb_sel),
    .o_rom_wb_adr (rom_wb_adr),
    .o_rom_wb_dat (rom_wb_wdata),
    .i_rom_wb_dat (rom_wb_rdata),
    .i_rom_wb_ack (rom_wb_ack),

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