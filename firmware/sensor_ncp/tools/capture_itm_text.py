#!/usr/bin/env python3

import argparse
import os
import queue
import sys
import time

import serial


DEFAULT_SDK_DIR = "/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07"


def add_itm_parser_paths():
    sdk_dir = os.environ.get("SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR", DEFAULT_SDK_DIR)
    sys.path.extend(
        [
            os.path.join(sdk_dir, "tools/log/tiutils/core"),
            os.path.join(sdk_dir, "tools/log/tiutils/streams/itm"),
        ]
    )


def main():
    parser = argparse.ArgumentParser(description="Capture and decode CC1352P7 ITM text on the XDS110 aux port.")
    parser.add_argument("port", help="Auxiliary COM port from the XDS110, for example /dev/cu.usbmodemLS41069U4")
    parser.add_argument("baud", type=int, nargs="?", default=3000000, help="ITM baud rate")
    parser.add_argument("--seconds", type=float, default=8.0, help="Capture duration")
    args = parser.parse_args()

    add_itm_parser_paths()
    from tilogger_itm_transport.itm_framer import ITMFramer

    frame_queue = queue.Queue()
    framer = ITMFramer(frame_queue)
    line_buf = bytearray()
    raw_len = 0

    with serial.Serial(args.port, args.baud, timeout=0.05) as ser:
        start = time.time()
        pending = bytearray()

        while time.time() - start < args.seconds:
            chunk = ser.read(4096)
            if not chunk:
                continue

            raw_len += len(chunk)
            pending.extend(chunk)
            pending = framer.parse(pending)

            while not frame_queue.empty():
                frame = frame_queue.get()

                if getattr(frame, "port", None) is None:
                    print(frame)
                    continue

                if frame.port.name == "STIM_INFO":
                    print("ITM reset token")
                    line_buf.clear()
                    continue

                if frame.port.name != "STIM_RAW0":
                    print(frame)
                    continue

                for byte in frame.data:
                    if byte == 0xAA:
                        continue
                    line_buf.append(byte)
                    if byte == 0x0A:
                        print(line_buf.decode("ascii", errors="replace").rstrip())
                        line_buf.clear()

    print(f"raw_bytes={raw_len}")


if __name__ == "__main__":
    main()
