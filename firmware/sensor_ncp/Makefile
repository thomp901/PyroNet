SHELL := /bin/zsh

ROOT := $(CURDIR)
BUILD_DIR := freertos/ticlang
OUT := $(BUILD_DIR)/sensor_ncp.out

SDK_DIR ?= /Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07
SYSCONFIG_TOOL ?= /Applications/ti/ccs2050/ccs/utils/sysconfig_1.27.0/sysconfig_cli.sh
TICLANG_ARMCOMPILER ?= /Applications/ti/ti-cgt-armllvm_5.1.0.LTS

FLASH_TOOL := /Applications/ti/uniflash_9.5.0/dslite.sh
FLASH_CONFIG := tools/cc1352p7_2pin_cJTAG_XDS110.ccxml

SWO_BAUD ?= 3000000
SWO_PORT ?=
SWO_PORT_GLOB ?= /dev/cu.usbmodem*

MAKE_BUILD = $(MAKE) -C $(BUILD_DIR) \
	SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR=$(SDK_DIR) \
	SYSCONFIG_TOOL=$(SYSCONFIG_TOOL) \
	TICLANG_ARMCOMPILER=$(TICLANG_ARMCOMPILER)

ifneq ($(strip $(SWO_PORT)),)
SWO_PORT_ARG := --port $(SWO_PORT)
endif

.PHONY: help build clean flash run capture bootlog artifact watch

help:
	@printf "Targets:\n"
	@printf "  make build    Build $(OUT)\n"
	@printf "  make flash    Build and flash $(OUT)\n"
	@printf "  make run      Start the persistent SWO logger only\n"
	@printf "  make watch    Hold the debug probe open and stream SWO in the terminal\n"
	@printf "  make capture  One-shot 30 second SWO capture\n"
	@printf "  make bootlog  Capture SWO while flashing to force a fresh boot\n"
	@printf "  make clean    Clean the ticlang build output\n"
	@printf "  make artifact Print the output artifact path\n"
	@printf "\nOverrides:\n"
	@printf "  SDK_DIR=<path>\n"
	@printf "  SYSCONFIG_TOOL=<path>\n"
	@printf "  TICLANG_ARMCOMPILER=<path>\n"
	@printf "  SWO_PORT=/dev/cu.usbmodem...\n"
	@printf "  SWO_PORT_GLOB=/dev/cu.usbmodem*\n"
	@printf "  SWO_BAUD=3000000\n"

build:
	$(MAKE_BUILD) all

clean:
	$(MAKE_BUILD) clean

flash: build
	$(FLASH_TOOL) --config=$(FLASH_CONFIG) -e $(OUT)

run:
	mkdir -p logs/swo
	python3 tools/swo_log_daemon.py $(SWO_PORT_ARG) --baud $(SWO_BAUD) --log-dir logs/swo

watch:
	chmod +x tools/watch_swo.sh
	./tools/watch_swo.sh $(SWO_PORT_ARG) --port-glob "$(SWO_PORT_GLOB)"

capture:
	python3 tools/capture_itm_text.py $(if $(strip $(SWO_PORT)),$(SWO_PORT),/dev/cu.usbmodemLS41069U4) $(SWO_BAUD) --seconds 30

bootlog:
	chmod +x tools/capture_boot_log.sh
	./tools/capture_boot_log.sh $(if $(strip $(SWO_PORT)),$(SWO_PORT),/dev/cu.usbmodemLS41069U4) 30

artifact:
	@printf "%s/%s\n" "$(ROOT)" "$(OUT)"
