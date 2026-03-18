# Minimal Makefile for bare-metal RV32I + ILP32
#
# Expected files:
#   crt0.S
#   link.ld
#   common .c / .S files in ./sw such as drivers
#   solution-specific .c files in ../problems/$(SOL)/
#
# Default outputs:
#   $(BUILD_DIR)/sw/$(SOL).elf
#   $(BUILD_DIR)/sw/$(SOL).bin
#   $(BUILD_DIR)/sw/$(SOL).dis
#   $(BUILD_DIR)/rtl/eselproc_wb_rom.sv

TARGET := $(SOL)

RISCV_PREFIX ?= $(RISCV_PREFIX_VAR)
# see: ../docker/.env

CC      := $(RISCV_PREFIX)gcc
OBJCOPY := $(RISCV_PREFIX)objcopy
OBJDUMP := $(RISCV_PREFIX)objdump
SIZE    := $(RISCV_PREFIX)size
PYTHON  := python3

# Shared build root passed from top-level Makefile.
# Fallback makes this file usable standalone too.
BUILD_DIR     ?= $(abspath ../build)
SW_BUILD_DIR  := $(BUILD_DIR)/sw
RTL_BUILD_DIR := $(BUILD_DIR)/rtl

ROMGEN     ?= $(abspath ../utils/python/gen_elf2svrom.py)
ROM_MODULE := eselproc_wb_rom
ROM_WIDTH  := 32

ARCH := -march=rv32i -mabi=ilp32

CFLAGS := $(ARCH) \
          -nostdlib -nostartfiles -ffreestanding \
          -fdata-sections -ffunction-sections \
          -msmall-data-limit=0 \
          -mno-relax \
          -O3 -Wall -Wextra \
          -I. \
          -flto

ASFLAGS := $(ARCH) \
           -nostdlib -nostartfiles -ffreestanding \
           -msmall-data-limit=0 \
           -mno-relax \
           -flto

LDFLAGS := $(ARCH) \
           -nostdlib -nostartfiles -ffreestanding \
           -Wl,--gc-sections \
           -Wl,-T,link.ld \
           -Wl,-Map,$(SW_BUILD_DIR)/$(TARGET).map \
           -flto

# Common sources in ./sw
C_SRCS := $(wildcard *.c)
S_SRCS := $(wildcard *.S)

# Solution-specific sources in ./sw/$(SOL)
C_SRCS_SOL := $(wildcard ../problems/$(SOL)/*.c)
S_SRCS_SOL := $(wildcard ../problems/$(SOL)/*.S)

# Object files mirrored under $(SW_BUILD_DIR)
OBJS := \
	$(patsubst %.c,$(SW_BUILD_DIR)/%.o,$(C_SRCS)) \
	$(patsubst %.S,$(SW_BUILD_DIR)/%.o,$(S_SRCS)) \
	$(patsubst %.c,$(SW_BUILD_DIR)/%.o,$(C_SRCS_SOL)) \
	$(patsubst %.S,$(SW_BUILD_DIR)/%.o,$(S_SRCS_SOL))

ELF    := $(SW_BUILD_DIR)/$(TARGET).elf
BIN    := $(SW_BUILD_DIR)/$(TARGET).bin
DIS    := $(SW_BUILD_DIR)/$(TARGET).dis
ROM_SV := $(RTL_BUILD_DIR)/$(ROM_MODULE).sv

.PHONY: all clean rom disasm size

all: $(ELF) $(BIN) $(DIS)

rom: $(ROM_SV)

size: $(ELF)
	$(SIZE) $(ELF)

disasm: $(DIS)

$(SW_BUILD_DIR):
	mkdir -p $(SW_BUILD_DIR)

$(RTL_BUILD_DIR):
	mkdir -p $(RTL_BUILD_DIR)
	
# Ensure nested subdirs exist, e.g. build/sw/01_multiplier/
$(SW_BUILD_DIR)/%.o: %.c | $(SW_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(SW_BUILD_DIR)/%.o: %.S | $(SW_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJS) link.ld | $(SW_BUILD_DIR)
	$(CC) $(OBJS) $(LDFLAGS) -lgcc -o $@
	$(SIZE) $@

$(BIN): $(ELF) | $(SW_BUILD_DIR)
	$(OBJCOPY) -O binary $< $@

$(DIS): $(ELF) | $(SW_BUILD_DIR)
	$(OBJDUMP) -d -S -z -M no-aliases $< > $@

$(ROM_SV): $(ELF) $(ROMGEN) | $(RTL_BUILD_DIR)
	$(PYTHON) $(ROMGEN) \
		$< \
		$@ \
		--module-name $(ROM_MODULE) \
		--data-width $(ROM_WIDTH) \
		--annotate-asm \
		--objdump $(OBJDUMP)

clean:
	rm -rf $(SW_BUILD_DIR)