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
