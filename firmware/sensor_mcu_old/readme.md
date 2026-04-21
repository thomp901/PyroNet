# sensor_mcu

SWO logs are emitted on ITM stimulus port 0 over PA03. To read them through the
SI-DBG1015A in SWD mode, run:

```sh
"$HOME/.silabs/slt/installs/archive/Commander.app/Contents/MacOS/commander" \
  swo read \
  --device EFR32FG28A122F1024GM48 \
  --serialno 440365444 \
  --tif SWD \
  --swospeed 875000 \
  --timeout 5 \
  --endmarker "POST_INIT_RETRY"
```

Expected boot sequence:
`SWO_SELF_TEST: FG28_PA03_ITM_CH0 phase=EARLY`
`SWO_SELF_TEST: FG28_PA03_ITM_CH0 phase=POST_INIT`
`SWO_SELF_TEST: FG28_PA03_ITM_CH0 phase=POST_INIT_RETRY`

Use `--noreset` if the target is already running and you only want to attach to
an existing session.

Current firmware bring-up kept in-repo:

- SWO logging on `PA03`
- I2C bring-up on `I2C1` using `PD02/PD03` at `100 kHz`
- UART bring-up on `EUSART1` using `PC0/PC1` at `115200 baud`

`app_init()` logs the I2C and UART bring-up status over SWO, and emits a short
`sensor_mcu UART ready` banner on `EUSART1` TX during boot.
