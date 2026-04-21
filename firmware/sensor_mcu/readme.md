# sensor_mcu

This firmware exposes a dedicated SWO debug path through [debug_console.c](./debug_console.c)
and [debug_console.h](./debug_console.h). The implementation uses:

- ITM stimulus port `0` for application log output
- PA03 as the SWO pin
- NRZ SWO at a target bitrate of `875000`
- a stable boot-ready marker of `SWO_BOOT_READY`

The firmware emits two boot lines during `app_init()`:

- `SWO_BOOT backend=SWO port=0 speed=<actual>`
- `SWO_BOOT_READY`

The current sensor bring-up path also probes the shared I2C bus during `app_init()`:

- SPS30 at address `0x69`
- BME68x at address `0x76`

Probe output is emitted over the same log path from [sensor_bus.c](./sensor_bus.c):

- `SENSOR_FOUND name=SPS30 addr=0x69` when the SPS30 ACKs
- `SENSOR_PROBE name=SPS30 addr=0x69 status=<status>` when the SPS30 does not ACK cleanly
- `SENSORS_READY bme68x=<0|1> sps30=<0|1>` after both probes complete

The staged Sensirion driver sources live under [third_party/sps30](./third_party/sps30),
but those files are not currently linked by the generated target. The current build
only performs presence probing through [sensor_bus.c](./sensor_bus.c) and
[board_i2c.c](./board_i2c.c).

Use the repo `Makefile` for the normal host workflow:

```sh
make verify-swo
```

That will build, flash, and read SWO until `SWO_BOOT_READY` is observed.

To read SWO directly through the SI-DBG1015A in SWD mode, run:

```sh
"$HOME/.silabs/slt/installs/archive/Commander.app/Contents/MacOS/commander" \
  swo read \
  --device EFR32FG28A122F1024GM48 \
  --serialno 440365444 \
  --tif SWD \
  --swospeed 875000 \
  --timeout 5 \
  --endmarker "SWO_BOOT_READY"
```

Use `--noreset` if the target is already running and you only want to attach to
an existing session, or run:

```sh
make swo-attach TIMEOUT=5
```

If you add temporary SWO diagnostics while debugging, keep them short and
remove or reduce them once verification is complete.
