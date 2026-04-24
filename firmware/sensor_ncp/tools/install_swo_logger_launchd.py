#!/usr/bin/env python3

import argparse
import os
import plistlib
import subprocess
import sys
from pathlib import Path


LABEL = "com.pyronet.swo-logger"


def repo_root():
    return Path(__file__).resolve().parents[1]


def launch_agents_dir():
    return Path.home() / "Library" / "LaunchAgents"


def plist_path():
    return launch_agents_dir() / f"{LABEL}.plist"


def build_plist(port: str | None):
    root = repo_root()
    log_dir = root / "logs" / "swo"
    daemon_path = root / "tools" / "swo_log_daemon.py"
    python_path = Path(sys.executable)

    program_arguments = [
        str(python_path),
        str(daemon_path),
        "--log-dir",
        str(log_dir),
    ]
    if port:
        program_arguments.extend(["--port", port])

    env = {}
    sdk_dir = os.environ.get("SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR")
    if sdk_dir:
        env["SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR"] = sdk_dir

    plist = {
        "Label": LABEL,
        "ProgramArguments": program_arguments,
        "WorkingDirectory": str(root),
        "RunAtLoad": True,
        "KeepAlive": True,
        "StandardOutPath": str(log_dir / "launchd.stdout.log"),
        "StandardErrorPath": str(log_dir / "launchd.stderr.log"),
    }
    if env:
        plist["EnvironmentVariables"] = env
    return plist


def install(port: str | None, load: bool):
    root = repo_root()
    log_dir = root / "logs" / "swo"
    log_dir.mkdir(parents=True, exist_ok=True)
    launch_agents_dir().mkdir(parents=True, exist_ok=True)

    with plist_path().open("wb") as handle:
        plistlib.dump(build_plist(port), handle)

    print(f"Wrote {plist_path()}")
    if load:
        uid = str(os.getuid())
        subprocess.run(["launchctl", "bootout", f"gui/{uid}", str(plist_path())], check=False)
        subprocess.run(["launchctl", "bootstrap", f"gui/{uid}", str(plist_path())], check=True)
        subprocess.run(["launchctl", "kickstart", "-k", f"gui/{uid}/{LABEL}"], check=True)
        print(f"Loaded {LABEL}")


def uninstall(unload: bool):
    uid = str(os.getuid())
    if unload and plist_path().exists():
        subprocess.run(["launchctl", "bootout", f"gui/{uid}", str(plist_path())], check=False)

    if plist_path().exists():
        plist_path().unlink()
        print(f"Removed {plist_path()}")
    else:
        print(f"Missing {plist_path()}")


def main():
    parser = argparse.ArgumentParser(description="Install or remove the persistent PyroNet SWO launch agent.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    install_parser = subparsers.add_parser("install")
    install_parser.add_argument("--port", help="Pin the logger to a specific XDS110 aux port.")
    install_parser.add_argument("--no-load", action="store_true", help="Write the plist without loading it.")

    subparsers.add_parser("print")

    uninstall_parser = subparsers.add_parser("uninstall")
    uninstall_parser.add_argument("--no-unload", action="store_true", help="Remove the plist without calling launchctl.")

    args = parser.parse_args()

    if args.command == "install":
        install(args.port, not args.no_load)
    elif args.command == "print":
        plistlib.dump(build_plist(None), sys.stdout.buffer)
    elif args.command == "uninstall":
        uninstall(not args.no_unload)


if __name__ == "__main__":
    main()
