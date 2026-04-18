TI Wi-SUN FAN CoAP Node Example Staging
======================================

This folder contains the TI `ns_coap_node_src` FreeRTOS example staged locally
for CCS import on macOS.

Import this project from:

- `freertos/ticlang/ns_coap_node_src_LP_CC1352P7_1_freertos_ticlang.projectspec`

The project metadata is pinned to TI Arm Clang `3.2.1`, which is installed on
this machine under:

- `/Applications/ti/ti-cgt-armllvm_3.2.1.LTS`

The editable application sources are copied into `application/` in this folder.
Most TI Wi-SUN stack sources still come from the installed SimpleLink SDK:

- `/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07`

The staged project still targets TI's `LP_CC1352P7_1` board configuration. For
a custom CC1352P7 board, you will still need to adjust board, pin, and radio
configuration after import.
