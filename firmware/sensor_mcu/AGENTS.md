# AGENTS.md

## Purpose

This repo builds a bare-metal Silicon Labs firmware for `EFR32FG28A122F1024GM48`
using Simplicity SDK `2025.6.2`.

The project source of truth is [sensor_mcu.slcp](./sensor_mcu.slcp).

This checkout is still close to the Simplicity "empty project" template:

- `main.c` is the SDK-owned superloop entrypoint.
- `app.c` / `app.h` are the current user hook points.
- most initialization is generated into `autogen/` and wired by `cmake_gcc/sensor_mcu.cmake`.

## Navigation Map

Read these first when orienting yourself:

- [sensor_mcu.slcp](./sensor_mcu.slcp): component selection, device target, SDK version.
- [main.c](./main.c): control-flow entrypoint into `app_init()` and `app_process_action()`.
- [app.c](./app.c): primary user-editable application logic.
- [debug_console.c](./debug_console.c): manual SWO/ITM backend and boot-marker implementation.
- [debug_console.h](./debug_console.h): SWO marker names, port selection, and public API.
- [autogen/sl_event_handler.c](./autogen/sl_event_handler.c): generated platform/service init chain.
- [cmake_gcc/sensor_mcu.cmake](./cmake_gcc/sensor_mcu.cmake): generated build graph, include paths, defines, and linked sources.

Top-level directories and their roles:

- `config/`: generated configuration headers for clocks, DCDC, EMU, memory, and pins.
- `autogen/`: generated SDK glue such as the linker script, component catalog, and init handlers.
- `cmake_gcc/`: CMake entrypoint, presets, toolchain file, and generated target wiring.
- `third_party/`: sensor/vendor libraries staged in the repo; not all of them are currently compiled into this target.
- `simplicity_sdk_2025.6.2/`: vendored SDK snapshot used by this checkout.

## Start Here By Task

If you are changing application behavior:

- start in [app.c](./app.c)
- then inspect [main.c](./main.c) to understand when the hook runs

If you are changing SWO logging, readback markers, or host-side SWO verification:

- start in [debug_console.c](./debug_console.c)
- inspect [debug_console.h](./debug_console.h) for the stable marker strings and SWO settings
- inspect [Makefile](./Makefile) for the local SWO capture workflow

If you are changing startup, clocks, or low-level init:

- start in [autogen/sl_event_handler.c](./autogen/sl_event_handler.c)
- then inspect [config/sl_clock_manager_tree_config.h](./config/sl_clock_manager_tree_config.h)
- and [config/sl_clock_manager_oscillator_config.h](./config/sl_clock_manager_oscillator_config.h)

If you are changing power or debug retention behavior:

- inspect [config/sl_device_init_dcdc_config.h](./config/sl_device_init_dcdc_config.h)
- inspect [config/sl_device_init_emu_config.h](./config/sl_device_init_emu_config.h)

If you are changing linker or memory placement:

- inspect [autogen/linkerfile.ld](./autogen/linkerfile.ld)
- inspect [config/sl_memory_manager_region_config.h](./config/sl_memory_manager_region_config.h)

If you are changing what gets compiled or linked:

- inspect [cmake_gcc/CMakeLists.txt](./cmake_gcc/CMakeLists.txt) for manual extension points
- inspect [cmake_gcc/sensor_mcu.cmake](./cmake_gcc/sensor_mcu.cmake) for the generated source list and flags

If you need device register definitions or startup code:

- use the vendored SDK under [simplicity_sdk_2025.6.2/platform/Device/SiliconLabs/EFR32FG28](./simplicity_sdk_2025.6.2/platform/Device/SiliconLabs/EFR32FG28)
- especially `Include/efr32fg28a122f1024gm48.h`, `Source/startup_efr32fg28.c`, and `Source/system_efr32fg28.c`

## Editable Vs Generated

Treat these as normal hand-edited project files:

- [app.c](./app.c)
- [app.h](./app.h)
- [debug_console.c](./debug_console.c)
- [debug_console.h](./debug_console.h)
- [main.c](./main.c) if you intentionally want to diverge from the template
- [Makefile](./Makefile)
- [readme.md](./readme.md)
- manual additions in [cmake_gcc/CMakeLists.txt](./cmake_gcc/CMakeLists.txt)

Treat these as generated or SDK-managed and edit only with care:

- everything under `autogen/`
- most files under `config/`
- [cmake_gcc/sensor_mcu.cmake](./cmake_gcc/sensor_mcu.cmake)

If you hand-edit generated files, expect Simplicity regeneration to overwrite them.

## Third-Party Code

The repo contains staged sensor libraries:

- `third_party/bme68x/`
- `third_party/bsec2/`

Important detail: those sources are present in the tree, but they are not listed in
the current generated source list in [cmake_gcc/sensor_mcu.cmake](./cmake_gcc/sensor_mcu.cmake).
Do not assume they are active in the build until you confirm the target links them.

## Build Outputs

Important generated artifacts:

- ELF: [cmake_gcc/build/base/sensor_mcu.out](./cmake_gcc/build/base/sensor_mcu.out)
- HEX: [cmake_gcc/build/base/sensor_mcu.hex](./cmake_gcc/build/base/sensor_mcu.hex)
- MAP: [cmake_gcc/build/base/sensor_mcu.map](./cmake_gcc/build/base/sensor_mcu.map)

## Build And Flash

Use the repo-root [Makefile](./Makefile) for the normal local workflow:

- `make build`
- `make probe`
- `make info`
- `make flash`
- `make swo`
- `make swo-attach`
- `make verify-swo`

Default assumptions in that file:

- device: `EFR32FG28A122F1024GM48`
- build dir: `cmake_gcc/build`
- image: `cmake_gcc/build/base/sensor_mcu.hex`
- default J-Link serial fallback on this machine: `440365444`
- default SWO target bitrate: `875000`
- default SWO end marker: `SWO_BOOT_READY`

Override adapter or SWD clock as needed:

- `make flash SERIALNO=440365444`
- `make flash SPEED=1000`
- `make swo TIMEOUT=10`
- `make swo-attach TIMEOUT=5`
- `make verify-swo SPEED=100`

If the `Makefile` is not yet present in a given checkout, the lower-level local build entrypoint is
the CMake preset in [cmake_gcc/CMakePresets.json](./cmake_gcc/CMakePresets.json):

1. `cmake --workflow --preset project`

That preset configures into `cmake_gcc/build/` and builds the `base` configuration.

Toolchain paths are hard-coded for this machine in [cmake_gcc/toolchain.cmake](./cmake_gcc/toolchain.cmake).

## SWO Validation Policy

For firmware changes in this repo, always use SWO readback as the primary
validation, verification, and debugging path when hardware is available.

For implementation tasks, use SWO not only for final verification but also as
the default live-debug path while bringing changes up on hardware.

Required defaults:

- use SWD mode, not 4-wire JTAG
- use PA03 / SWO only for log capture on this target
- do not substitute UART, VCOM, RTT, or other serial paths when validating SWO-capable firmware changes
- after code changes that affect boot, logging, clocks, debug, initialization, or runtime behavior, build, flash, and verify behavior with a live SWO read
- when adding temporary diagnostics for bring-up or debugging, prefer short SWO markers that can be read back from the host
- when debugging an implementation, add or adjust SWO markers first before reaching for alternate host-side inspection paths
- remove or reduce temporary SWO diagnostics once the implementation is verified, unless the user asks to keep them

Minimum verification workflow for implementation tasks:

1. build the firmware
2. flash the firmware
3. run a live SWO read and confirm the expected marker or behavior on host

Recommended debug loop for implementation work:

1. add a short SWO marker at the point you need to observe
2. build and flash the firmware
3. read SWO live from the host and confirm the exact marker ordering or payload
4. refine the implementation and repeat until behavior matches expectations

If SWO capture is blocked by missing hardware, probe access, or host tooling,
state the blocker explicitly. Do not claim full verification without an actual
SWO readback from the SI-DBG1015A.

## Device-Specific SDK Files

These are the first SDK files to inspect for `EFR32FG28A122F1024GM48`.

In the vendored copy in this repo:

- [simplicity_sdk_2025.6.2/platform/Device/SiliconLabs/EFR32FG28](./simplicity_sdk_2025.6.2/platform/Device/SiliconLabs/EFR32FG28)

In the installed SDK path referenced by generated build files:

- `~/.silabs/slt/installs/conan/p/simpleb526998f4a4d/p/platform/Device/SiliconLabs/EFR32FG28`
