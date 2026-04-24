#!/usr/bin/env python3

import argparse
import glob
import os
import queue
import signal
import sys
import time
from datetime import datetime
from pathlib import Path

import serial


DEFAULT_SDK_DIR = "/Users/diegosmacbook/ti/simplelink_cc13xx_cc26xx_sdk_8_32_00_07"
DEFAULT_BAUD = 3000000
DEFAULT_PORT_GLOB = "/dev/cu.usbmodem*"
DEFAULT_RETRY_SECONDS = 2.0

running = True
emit_stdout = True


def add_itm_parser_paths():
    sdk_dir = os.environ.get("SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR", DEFAULT_SDK_DIR)
    sys.path.extend(
        [
            os.path.join(sdk_dir, "tools/log/tiutils/core"),
            os.path.join(sdk_dir, "tools/log/tiutils/streams/itm"),
        ]
    )


def signal_handler(signum, _frame):
    global running
    running = False
    if emit_stdout:
        print(f"LOGGER signal={signum} action=shutdown", flush=True)


class DailyLogWriter:
    def __init__(self, log_dir: Path):
        self.log_dir = log_dir
        self.log_dir.mkdir(parents=True, exist_ok=True)
        self.current_day = None
        self.handle = None

    def _log_path_for_now(self):
        stamp = datetime.now().strftime("%Y-%m-%d")
        return self.log_dir / f"swo_{stamp}.log"

    def _ensure_open(self):
        path = self._log_path_for_now()
        day = path.name
        if self.handle is not None and self.current_day == day:
            return

        if self.handle is not None:
            self.handle.close()

        self.handle = path.open("a", encoding="utf-8")
        self.current_day = day

    def write_line(self, line: str):
        self._ensure_open()
        timestamp = datetime.now().astimezone().isoformat(timespec="milliseconds")
        self.handle.write(f"{timestamp} {line}\n")
        self.handle.flush()

    def close(self):
        if self.handle is not None:
            self.handle.close()
            self.handle = None
            self.current_day = None


def pick_port(explicit_port: str | None, port_glob: str, last_port: str | None):
    if explicit_port:
        return explicit_port

    candidates = sorted(glob.glob(port_glob))
    if not candidates:
        return None

    if last_port in candidates:
        return last_port

    return candidates[-1]


def emit(writer: DailyLogWriter, line: str):
    if emit_stdout:
        print(line, flush=True)
    writer.write_line(line)


def capture_once(writer: DailyLogWriter, port: str, baud: int):
    add_itm_parser_paths()
    from tilogger_itm_transport.itm_framer import ITMFramer

    frame_queue = queue.Queue()
    framer = ITMFramer(frame_queue)
    pending = bytearray()
    line_buf = bytearray()

    emit(writer, f"LOGGER connected port={port} baud={baud}")

    # Poll `in_waiting` on macOS instead of relying on pyserial's `read()`
    # select loop, which can report false EOFs on XDS110 CDC ports.
    with serial.Serial(port, baud, timeout=0) as ser:
        while running:
            waiting = ser.in_waiting
            if waiting <= 0:
                time.sleep(0.05)
                continue

            chunk = ser.read(waiting)
            if not chunk:
                continue

            pending.extend(chunk)
            pending = framer.parse(pending)

            while not frame_queue.empty():
                frame = frame_queue.get()

                if getattr(frame, "port", None) is None:
                    emit(writer, str(frame))
                    continue

                if frame.port.name == "STIM_INFO":
                    line_buf.clear()
                    emit(writer, "ITM reset token")
                    continue

                if frame.port.name != "STIM_RAW0":
                    emit(writer, str(frame))
                    continue

                for byte in frame.data:
                    if byte == 0xAA:
                        continue
                    line_buf.append(byte)
                    if byte == 0x0A:
                        emit(writer, line_buf.decode("ascii", errors="replace").rstrip())
                        line_buf.clear()


def main():
    global emit_stdout
    parser = argparse.ArgumentParser(description="Continuously capture and persist decoded SWO/ITM text.")
    parser.add_argument("--port", help="Explicit XDS110 aux port. If omitted, auto-detect from --port-glob.")
    parser.add_argument("--port-glob", default=DEFAULT_PORT_GLOB, help="Glob used when auto-detecting the aux port.")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="ITM baud rate.")
    parser.add_argument("--retry-seconds", type=float, default=DEFAULT_RETRY_SECONDS, help="Reconnect retry interval.")
    parser.add_argument("--quiet-stdout", action="store_true", help="Write only to the log file, not stdout.")
    parser.add_argument(
        "--log-dir",
        default=str(Path(__file__).resolve().parents[1] / "logs" / "swo"),
        help="Directory where daily SWO logs are written.",
    )
    args = parser.parse_args()
    emit_stdout = not args.quiet_stdout

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    writer = DailyLogWriter(Path(args.log_dir))
    last_port = None

    try:
        while running:
            port = pick_port(args.port, args.port_glob, last_port)
            if port is None:
                emit(writer, f"LOGGER waiting_for_port glob={args.port_glob}")
                time.sleep(args.retry_seconds)
                continue

            last_port = port
            try:
                capture_once(writer, port, args.baud)
            except serial.SerialException as exc:
                emit(writer, f"LOGGER serial_error port={port} error={exc}")
                time.sleep(args.retry_seconds)
            except FileNotFoundError:
                emit(writer, f"LOGGER port_disappeared port={port}")
                time.sleep(args.retry_seconds)
            except KeyboardInterrupt:
                break
            except Exception as exc:  # pragma: no cover
                emit(writer, f"LOGGER unexpected_error port={port} error={exc}")
                time.sleep(args.retry_seconds)
    finally:
        writer.close()


if __name__ == "__main__":
    main()
