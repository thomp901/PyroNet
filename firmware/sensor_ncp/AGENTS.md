Actual target hardware:

- This firmware targets the `BDE-MB1352P71` module mounted on a custom carrier board.
- It does not target the TI `LP_CC1352P7_1` LaunchPad directly.
- The SysConfig file may still reference the LaunchPad board as a generic device context, but physical pin mappings must follow the custom board and BDE module, not LaunchPad aliases.

Hardware-specific constraints:

- For `BDE-MB1352P71XX32` variants, `DIO_8`, `DIO_9`, `DIO_10`, and `DIO_20` are used by the on-module SPI flash and should not be repurposed.
- `DIO_29` and `DIO_30` are used internally by the BDE module RF switch and should not be used as normal GPIOs.
- The BDE module requires `XOSC Cap Array Delta = 0x00`; this is configured in `freertos/sensor_ncp.syscfg` through the `CCFG.xoscCapArray` and `CCFG.xoscCapArrayDelta` settings.
- If RF support is added later, use the BDE RF-switch behavior rather than TI LaunchPad defaults. A commented stub lives in `user_rf_switch_cfg.c`.

Verified build process:

```sh
cd /Users/diegosmacbook/Documents/PyroNet/firmware/sensor_ncp
make -C freertos/ticlang \
  SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR=/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07 \
  SYSCONFIG_TOOL=/Applications/ti/ccs2050/ccs/utils/sysconfig_1.27.0/sysconfig_cli.sh \
  TICLANG_ARMCOMPILER=/Applications/ti/ti-cgt-armllvm_5.1.0.LTS \
  clean all
```

Build artifact:

- `freertos/ticlang/sensor_ncp.out`

Verified flash process:

```sh
cd /Users/diegosmacbook/Documents/PyroNet/firmware/sensor_ncp
/Applications/ti/uniflash_9.5.0/dslite.sh \
  --config=/Applications/ti/uniflash_9.5.0/deskdb/content/TICloudAgent/osx/ccs_base/arm/cc1352p7_2pin_cJTAG_XDS110.ccxml \
  -e \
  freertos/ticlang/sensor_ncp.out
```

Verified SWO / ITM debug process:

- Use SWO / ITM traces as a normal debugging tool when investigating firmware behavior on this target.
- On this custom carrier board, the XDS110 debug header `SWO/TDO` signal is routed to the module's `JTAG_TDO`, which is `DIO_16`.
- Do not assume LaunchPad-style SWO routing such as `DIO_18`; that is not the correct trace pin on this hardware.
- The verified working debugger transport is XDS110 `2-pin cJTAG` with the aux COM port mapped to the target `TDO` pin, not a pure SWD attach.
- The repo-local XDS110 config for this is `tools/cc1352p7_2pin_cJTAG_XDS110.ccxml`.
- The repo-local ITM text capture helper is `tools/capture_itm_text.py`.

Verified SWO / ITM capture example:

```sh
cd /Users/diegosmacbook/Documents/PyroNet/firmware/sensor_ncp
python3 tools/capture_itm_text.py /dev/cu.usbmodemLS41069U4 3000000 --seconds 8
```

- The exact macOS device node may vary between hosts or reconnects; identify the XDS110 aux port before capture if needed.
- A verified decoded boot trace from this firmware is:
  `SWO_SELF_TEST: CC1352P7_DIO16_ITM_CH0 phase=BOOT pulse_dio=28`

Reference:

- If CoAP and Wi-SUN support are enabled later for this custom board, do not import LaunchPad files directly, but use the TI LaunchPad example here as a reference for the relevant pieces: `/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07/examples/rtos/LP_CC1352P7_1/ti_wisunfan/ns_coap_node_src`
- The target deployment is a Linux border router using a Silicon Labs dev kit as the RCP.
- A confirmed-working Wi-SUN client image that connects to that border router lives at `/Users/diegosmacbook/Documents/PyroNet/firmware/wisun_soc_cli`; use it as behavioral and configuration reference material when aligning future client support.
- The corresponding RCP image folder for that setup is `/Users/diegosmacbook/Documents/PyroNet/firmware/wisun_rcp`.
- Do not modify any reference tree directly, including the TI SDK example, `wisun_soc_cli`, `wisun_rcp`, or any linked SDK content outside this repository.
- If an SDK file or other externally linked file must be changed, first copy it into this `sensor_ncp` directory, update the project to link against the in-repo copy, and only then modify that copied file.
- Files outside `/Users/diegosmacbook/Documents/PyroNet/firmware/sensor_ncp` are read-only for this project and must not be edited in place.
- Any future adaptation must preserve the custom carrier-board pin mapping and BDE-module hardware constraints documented above rather than TI LaunchPad defaults.
