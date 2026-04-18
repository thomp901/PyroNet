# AGENTS.md

## Purpose

This repo stages and builds the TI Wi-SUN FAN `ns_coap_node_src` FreeRTOS
example for a `CC1352P7` target.

The current local setup is:

- target family: `CC1352P7`
- RTOS variant: `freertos`
- compiler: TI Arm Clang `3.2.1`
- SDK install: `/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07`
- working XDS110 serial on this machine: `LS41069U`

The editable app sources are staged in this repo under [application](./application).
Most TI Wi-SUN stack sources still resolve from the installed SimpleLink SDK.

## Useful Navigation

- App sources: [application](./application)
- FreeRTOS syscfg: [freertos/ti_wisunfan_coap_node.syscfg](./freertos/ti_wisunfan_coap_node.syscfg)
- TI-Clang projectspec: [freertos/ticlang/ns_coap_node_src_LP_CC1352P7_1_freertos_ticlang.projectspec](./freertos/ticlang/ns_coap_node_src_LP_CC1352P7_1_freertos_ticlang.projectspec)
- TI-Clang makefile: [freertos/ticlang/makefile](./freertos/ticlang/makefile)
- Local UniFlash target config: [tools/cc1352p7_2pin_cJTAG_XDS110.ccxml](./tools/cc1352p7_2pin_cJTAG_XDS110.ccxml)

Important generated artifacts:

- ELF: [freertos/ticlang/ns_coap_node_src.out](./freertos/ticlang/ns_coap_node_src.out)
- HEX: [freertos/ticlang/ns_coap_node_src.hex](./freertos/ticlang/ns_coap_node_src.hex)
- MAP: [freertos/ticlang/ns_coap_node_src.map](./freertos/ticlang/ns_coap_node_src.map)

## Clean Build

Do not rely on the SDK defaults in `imports.mak` for this project. On this
machine they point to older CCS and compiler paths.

Use this exact build command:

```sh
make -C freertos/ticlang clean all \
  SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR=/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07 \
  TICLANG_ARMCOMPILER=/Applications/ti/ti-cgt-armllvm_3.2.1.LTS \
  SYSCONFIG_TOOL=/Applications/ti/ccs2050/ccs/utils/sysconfig_1.27.0/sysconfig_cli.sh
```

Notes:

- `cgtVersion` in the projectspec is pinned to `3.2.1`
- SysConfig generation succeeded with CCS `20.5.0` / SysConfig `1.27.0`
- the build output is `freertos/ticlang/ns_coap_node_src.out`

## Clean Flash

Use UniFlash's bundled `DSLite`, not the CCS `20.5` DebugServer copy.

The CCS copy on this machine is missing the `CC26xx` flash library and may fail
with missing `libFlashCC26xx.dylib`. UniFlash has the correct runtime.

If the first flash attempt fails to claim the XDS110, toggle reset through the
probe once and retry:

```sh
/Applications/ti/ccs2050/ccs/ccs_base/common/uscif/xds110/xds110reset -s LS41069U
```

Then flash with:

```sh
/Applications/ti/uniflash_9.5.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite \
  flash \
  --config=/Users/diegosmacbook/Documents/PyroNet/firmware/sensor_ncp/tools/cc1352p7_2pin_cJTAG_XDS110.ccxml \
  -e \
  -f /Users/diegosmacbook/Documents/PyroNet/firmware/sensor_ncp/freertos/ticlang/ns_coap_node_src.out
```

Expected successful tail output:

- `Loading Program: ...ns_coap_node_src.out`
- `Setting PC to entry point.`
- `Success`

## Probe Notes

Probe enumeration verified locally with:

```sh
/Applications/ti/ccs2050/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
```

Observed good state:

- probe type: `XDS110 Embed with CMSIS-DAP`
- serial: `LS41069U`
- firmware: `3.0.0.41`
- mode: `Runtime`

## Known Failure Modes

### SDK build defaults are stale

`/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07/imports.mak`
defaults to:

- CCS `12.7`
- TI Arm Clang `3.2.2`

Override those paths explicitly when building.

### CCS DebugServer flash path is incomplete

The host CCS `20.5` install can connect far enough to start a load, but its
DebugServer tree is missing the `CC26xx` flash plugin. Use UniFlash's
DebugServer for programming.

### UniFlash probe claim can be flaky

UniFlash may initially fail with XDS110 error `-260` even when `xdsdfu -e`
sees the probe. A single `xds110reset` toggle was enough to recover the flash
path during this investigation.
