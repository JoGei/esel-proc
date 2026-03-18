# ./rtl/Makefile
RTL_DIR  := $(CURDIR)

BUILD_DIR ?= $(abspath ../build)
RTL_BUILD_DIR := $(BUILD_DIR)/rtl
FUSESOC_BUILD_DIR := $(BUILD_DIR)/fusesoc

SOL ?= 01_multiplier

BOOT_ROM  := $(BUILD_DIR)/rtl/eselproc_wb_rom.sv
SERV_FLAT := $(RTL_BUILD_DIR)/flat_serv.sv
SOLUTION  := $(RTL_BUILD_DIR)/Solution.v

.PHONY: build clean
build: $(SOLUTION)

$(RTL_BUILD_DIR):
	mkdir -p $@

$(SERV_FLAT): | $(RTL_BUILD_DIR)
	$(RTL_DIR)/gen_latchup_serv.sh "$(FUSESOC_BUILD_DIR)" "$@"

$(SOLUTION): $(SERV_FLAT) $(BOOT_ROM) | $(RTL_BUILD_DIR)
	python3 ../utils/python/merge_solution.py \
	  --template ../problems/$(SOL)/Solution.v.in \
	  --output $@ \
	  --module ESELPROC_DBUS_MODULE=eselproc_wb_dbus.sv \
	  --module ESELPROC_SRAM_MODULE=eselproc_wb_sram.sv \
	  --module ESELPROC_SOC_MODULE=eselproc_soc.sv \
	  --module BOOT_ROM_MODULE=$(BOOT_ROM) \
	  --module SERV_RF_TOP_MODULE=$(SERV_FLAT) \
	  --module LATCHUP_IF_MODULE=eselproc_latchup_interface.sv \
	  --module ESELPROC_C_PROGRAM=../problems/$(SOL)/eselproc_solution.c

clean:
	rm -rf $(RTL_BUILD_DIR)