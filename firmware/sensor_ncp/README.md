## sensor_ncp

This project builds the `sensor_ncp` firmware for the custom `BDE-MB1352P71` carrier-board target in this repository. It is not a LaunchPad project, even though SysConfig still uses `LP_CC1352P7_1` as the generic device context.

The current host boundary is a phase-1 binary UART protocol on `UART0`:

- UART pins: TX=`DIO_28`, RX=`DIO_27`
- UART settings: `115200 8N1`, no flow control
- Framing: `0xA5 0x5A`, version `0x01`, CRC-16/CCITT-FALSE
- Implemented message types: `HELLO`, `HELLO_ACK`, `PING`, `PONG`, `GET_STATUS`, `STATUS`, `ERROR`

The NCP owns only the link skeleton in this phase:

- waits for `HELLO`
- replies with `HELLO_ACK`
- enters `LINK_READY`
- responds to `PING` with `PONG`
- responds to `GET_STATUS` with placeholder `STATUS`

The production UART boundary is binary only. The earlier ASCII command bootstrap path is no longer the active transport.

## Hardware Notes

- Target module: `BDE-MB1352P71`
- Verified SWO / ITM trace pin: `DIO_16`
- Reserved BDE module pins must remain untouched:
  - `DIO_8`, `DIO_9`, `DIO_10`, `DIO_20` are used by on-module SPI flash
  - `DIO_29`, `DIO_30` are used by the RF switch
- XOSC cap settings for this module must remain:
  - `CCFG.xoscCapArray = true`
  - `CCFG.xoscCapArrayDelta = 0x00`

## Verified Build

```sh
cd /Users/diegosmacbook/Documents/PyroNet/firmware/sensor_ncp
make -C freertos/ticlang \
  SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR=/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07 \
  SYSCONFIG_TOOL=/Applications/ti/ccs2050/ccs/utils/sysconfig_1.27.0/sysconfig_cli.sh \
  TICLANG_ARMCOMPILER=/Applications/ti/ti-cgt-armllvm_5.1.0.LTS \
  clean all
```

Build output:

- `freertos/ticlang/sensor_ncp.out`

## Verified Flash

```sh
cd /Users/diegosmacbook/Documents/PyroNet/firmware/sensor_ncp
/Applications/ti/uniflash_9.5.0/dslite.sh \
  --config=tools/cc1352p7_2pin_cJTAG_XDS110.ccxml \
  -e \
  freertos/ticlang/sensor_ncp.out
```

## Wi-SUN Profile

The current image is configured for the running PyroNet border router:

- FAN: `1.1`
- Device role: `FFN`
- Network name: `PyroNet`
- Regulatory domain: `NA`
- Channel plan ID: `1`
- PHY mode ID: `0x02`
- Security: certificate-based

Repo-local credentials are expected at:

- `certs/ca_cert.pem`
- `certs/node_cert.pem`
- `certs/node_key.pem`

These are compiled through `application/wisun_certificates.h`.

## Verified Runtime Status

The current image has been verified to:

- boot on the custom board
- emit SWO / ITM trace on `DIO_16`
- start the TI Wi-SUN stack
- reach `CON_STATUS_CONNECTING` in local trace output
- join the running `PyroNet` border router

Join success was confirmed from the border-router side by observing:

- advertisement and configuration exchange
- DHCPv6 reply
- ARO registration for `fd12:3456::212:4b00:28ed:d802`
- RPL DAO and DAO-ACK

## SWO / ITM Capture

Verified helper:

```sh
cd /Users/diegosmacbook/Documents/PyroNet/firmware/sensor_ncp
python3 tools/capture_itm_text.py /dev/cu.usbmodemLS41069U4 3000000 --seconds 15
```

Notes:

- the exact `/dev/cu.usbmodem...` node may vary
- this hardware uses the XDS110 aux COM path mapped to target `TDO` on `DIO_16`
- `tools/capture_itm_text.py` includes a fallback decoder for this image's raw SWO byte pattern

## Reference Trees

These are reference-only and must not be edited in place:

- `../wisun_soc_cli`
- `../wisun_rcp`
- TI SDK example content under `/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07`
