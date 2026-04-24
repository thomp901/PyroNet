DEVICE ?= EFR32FG28A122F1024GM48
TARGET ?= sensor_mcu
CONFIG ?= base

CMAKE_DIR ?= cmake_gcc
BUILD_DIR ?= $(CMAKE_DIR)/build
IMAGE ?= $(BUILD_DIR)/$(CONFIG)/$(TARGET).hex
ELF ?= $(BUILD_DIR)/$(CONFIG)/$(TARGET).out
MAP ?= $(BUILD_DIR)/$(CONFIG)/$(TARGET).map
PROVISIONING_CONFIG ?= config/pyronet_app_provisioning_config.h

CMAKE ?= $(shell command -v cmake 2>/dev/null || printf '%s' "$(HOME)/.silabs/slt/installs/conan/p/cmake0491e58cd027f/p/CMake.app/Contents/bin/cmake")
COMMANDER ?= $(HOME)/.silabs/slt/installs/archive/Commander.app/Contents/MacOS/commander

SERIALNO ?= 440365444
SPEED ?= 1000
TIF ?= SWD
SWOSPEED ?= 875000
TIMEOUT ?= 5
ENDMARKER ?= SWO_BOOT_READY

.PHONY: build probe info flash flash-now stamp-boot-time-now swo swo-attach verify-swo test-risk clean

build: check-cmake
	cd $(CMAKE_DIR) && "$(CMAKE)" --workflow --preset project

probe: check-commander
	"$(COMMANDER)" adapter probe --serialno "$(SERIALNO)"

info: check-commander
	"$(COMMANDER)" device info --device "$(DEVICE)" --serialno "$(SERIALNO)" --tif "$(TIF)"

flash: build check-commander
	"$(COMMANDER)" flash "$(IMAGE)" --device "$(DEVICE)" --serialno "$(SERIALNO)" --tif "$(TIF)" --speed "$(SPEED)"

stamp-boot-time-now:
	@boot_unix_time_s="$$(date +%s)"; \
	awk -v boot_unix_time_s="$$boot_unix_time_s" '\
	/^#define PYRONET_APP_BOOT_UNIX_TIME_VALID[[:space:]]+/ { $$0 = "#define PYRONET_APP_BOOT_UNIX_TIME_VALID       1" } \
	/^#define PYRONET_APP_BOOT_UNIX_TIME_S[[:space:]]+/ { $$0 = "#define PYRONET_APP_BOOT_UNIX_TIME_S           " boot_unix_time_s "U" } \
	{ print }' "$(PROVISIONING_CONFIG)" > "$(PROVISIONING_CONFIG).tmp"; \
	mv "$(PROVISIONING_CONFIG).tmp" "$(PROVISIONING_CONFIG)"; \
	printf 'Stamped %s with PYRONET_APP_BOOT_UNIX_TIME_S=%s\n' "$(PROVISIONING_CONFIG)" "$$boot_unix_time_s"

flash-now: stamp-boot-time-now flash

swo: check-commander
	"$(COMMANDER)" swo read --device "$(DEVICE)" --serialno "$(SERIALNO)" --tif "$(TIF)" --swospeed "$(SWOSPEED)" --timeout "$(TIMEOUT)" --endmarker "$(ENDMARKER)"

swo-attach: check-commander
	"$(COMMANDER)" swo read --device "$(DEVICE)" --serialno "$(SERIALNO)" --tif "$(TIF)" --swospeed "$(SWOSPEED)" --timeout "$(TIMEOUT)" --noreset

verify-swo: flash
	@$(MAKE) swo SERIALNO="$(SERIALNO)" SPEED="$(SPEED)" TIF="$(TIF)" SWOSPEED="$(SWOSPEED)" TIMEOUT="$(TIMEOUT)" ENDMARKER="$(ENDMARKER)"

test-risk:
	mkdir -p build_host
	cc -std=c17 -Wall -Wextra -Werror -I. \
		tests/pyronet_risk_engine_test.c \
		services/risk/pyronet_risk_engine.c \
		-o build_host/pyronet_risk_engine_test
	./build_host/pyronet_risk_engine_test

clean: check-cmake
	cd $(CMAKE_DIR) && "$(CMAKE)" --build build --config "$(CONFIG)" --target clean

check-cmake:
	@test -x "$(CMAKE)" || { echo "CMake not found: $(CMAKE)"; exit 1; }

check-commander:
	@test -x "$(COMMANDER)" || { echo "Commander not found: $(COMMANDER)"; exit 1; }
