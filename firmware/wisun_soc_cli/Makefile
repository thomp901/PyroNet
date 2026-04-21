SHELL := /bin/sh

.DEFAULT_GOAL := help

PROJECT ?= wisun_soc_cli
DEVICE ?= EFR32ZG28B312F1024IM48

CMAKE ?= $(firstword \
	$(wildcard $(HOME)/.silabs/slt/installs/conan/p/cmake*/p/CMake.app/Contents/bin/cmake) \
	$(wildcard $(HOME)/.silabs/slt/installs/conan/p/cmake*/p/bin/cmake) \
	$(wildcard /Applications/CMake.app/Contents/bin/cmake) \
	$(wildcard /opt/homebrew/bin/cmake) \
	$(wildcard /usr/local/bin/cmake) \
	$(shell command -v cmake 2>/dev/null))
CMAKE_DIR ?= cmake_gcc
CONFIGURE_PRESET ?= project
BUILD_PRESET ?= default_config
WORKFLOW_PRESET ?= project
BUILD_DIR ?= $(CMAKE_DIR)/build/base

IMAGE ?= $(BUILD_DIR)/$(PROJECT).hex
S37_IMAGE ?= $(BUILD_DIR)/$(PROJECT).s37

COMMANDER ?= $(firstword \
	$(wildcard $(HOME)/.silabs/slt/installs/archive/Commander.app/Contents/MacOS/commander) \
	$(shell command -v commander 2>/dev/null) \
	$(shell command -v simplicity-commander 2>/dev/null))
AUTO_SERIALNO := $(shell if [ -n "$(COMMANDER)" ] && [ -x "$(COMMANDER)" ]; then "$(COMMANDER)" adapter list 2>&1 | sed -n 's/^[[:space:]]*serialNumber=\([0-9][0-9]*\)$$/\1/p' | head -n 1; fi)
SERIALNO ?= $(AUTO_SERIALNO)
SPEED ?=

SERIAL_ARG = $(if $(strip $(SERIALNO)),--serialno $(SERIALNO),)
SPEED_ARG = $(if $(strip $(SPEED)),--speed $(SPEED),)

.PHONY: help configure build rebuild workflow commander-check image-check s37-image-check probe info flash flash-s37 flash-masserase

help:
	@printf '%s\n' \
		'Wi-SUN CLI build and flash helpers' \
		'' \
		'Targets:' \
		'  make configure        Configure the CMake build in $(CMAKE_DIR)/build' \
		'  make build            Build $(PROJECT)' \
		'  make rebuild          Reconfigure and rebuild $(PROJECT)' \
		'  make workflow         Run the configured CMake workflow preset' \
		'  make probe            List adapters and probe the target' \
		'  make info             Read target device information' \
		'  make flash            Build if needed, then flash $(IMAGE)' \
		'  make flash-s37        Build if needed, then flash $(S37_IMAGE)' \
		'  make flash-masserase  Build if needed, mass erase, then flash $(IMAGE)' \
		'' \
		'Variables:' \
		'  SERIALNO=<sn>         Optional J-Link serial number; auto-detects the first adapter when blank' \
		'  SPEED=<kHz>           Optional SWD clock override' \
		'  DEVICE=<part>         Defaults to $(DEVICE)' \
		'  PROJECT=<name>        Defaults to $(PROJECT)' \
		'  IMAGE=<file>          Defaults to $(IMAGE)' \
		'  S37_IMAGE=<file>      Defaults to $(S37_IMAGE)' \
		'  COMMANDER=<path>      Override Commander binary path' \
		'' \
		'Examples:' \
		'  make build' \
		'  make flash' \
		'  make probe SERIALNO=446020568' \
		'  make flash SPEED=4000'

configure:
	@cd "$(CMAKE_DIR)" && "$(CMAKE)" --preset "$(CONFIGURE_PRESET)"

build: configure
	@cd "$(CMAKE_DIR)" && "$(CMAKE)" --build --preset "$(BUILD_PRESET)"

rebuild: configure build

workflow:
	@cd "$(CMAKE_DIR)" && "$(CMAKE)" --workflow --preset "$(WORKFLOW_PRESET)"

commander-check:
	@test -n "$(COMMANDER)" || { \
		echo "Commander not found. Set COMMANDER=/path/to/commander."; \
		exit 1; \
	}
	@test -x "$(COMMANDER)" || { \
		echo "Commander is not executable: $(COMMANDER)"; \
		exit 1; \
	}

image-check:
	@test -f "$(IMAGE)" || { \
		echo "Missing image: $(IMAGE)"; \
		echo "Run: make build"; \
		exit 1; \
	}

s37-image-check:
	@test -f "$(S37_IMAGE)" || { \
		echo "Missing image: $(S37_IMAGE)"; \
		echo "Run: make build"; \
		exit 1; \
	}

probe: commander-check
	@"$(COMMANDER)" adapter list
	@"$(COMMANDER)" adapter probe $(SERIAL_ARG)

info: commander-check
	@"$(COMMANDER)" device info $(SERIAL_ARG) $(SPEED_ARG) --device $(DEVICE)

flash: build commander-check image-check
	@"$(COMMANDER)" flash "$(IMAGE)" $(SERIAL_ARG) $(SPEED_ARG) --device $(DEVICE)

flash-s37: build commander-check s37-image-check
	@"$(COMMANDER)" flash "$(S37_IMAGE)" $(SERIAL_ARG) $(SPEED_ARG) --device $(DEVICE)

flash-masserase: build commander-check image-check
	@"$(COMMANDER)" flash "$(IMAGE)" $(SERIAL_ARG) $(SPEED_ARG) --device $(DEVICE) --masserase
