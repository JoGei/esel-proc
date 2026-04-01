# ./Makefile

ROOT_DIR  := $(CURDIR)
BUILD_DIR := $(ROOT_DIR)/build

SOL    ?= 01_multiplier
CPU	   ?= serv
ROMGEN ?= $(ROOT_DIR)/utils/python/gen_elf2svrom.py

.PHONY: all sw rtl clean
all: rtl

sw:
	$(MAKE) -C sw \
	  SOL=$(SOL) \
	  CPU=$(CPU) \
	  ROMGEN=$(ROMGEN) \
	  BUILD_DIR=$(BUILD_DIR) \
	  rom

rtl: sw
	$(MAKE) -C rtl \
	  SOL=$(SOL) \
	  CPU=$(CPU) \
	  BUILD_DIR=$(BUILD_DIR) \
	  build

# Verilator testbench for current SOL

TB_CPP := $(ROOT_DIR)/problems/$(SOL)/tb_veri.cpp
TOP    := Solution
VERI_BUILD := $(BUILD_DIR)/obj_dir

.PHONY: verilate run_tb test

verilate: $(BUILD_DIR)/rtl/Solution.v
	verilator -Wall -Wno-fatal --trace --cc $(BUILD_DIR)/rtl/Solution.v \
		--exe $(TB_CPP) --top-module $(TOP) -O3 --Mdir $(VERI_BUILD) \
		-CFLAGS "-O3"
	$(MAKE) -C $(VERI_BUILD) -f V$(TOP).mk V$(TOP)

run_tb:
	$(VERI_BUILD)/V$(TOP)

test: rtl verilate run_tb

clean:
	rm -rf $(BUILD_DIR)/rtl
	rm -rf $(BUILD_DIR)/sw
	rm -rf $(BUILD_DIR)/problems
	rm -rf $(VERI_BUILD)
