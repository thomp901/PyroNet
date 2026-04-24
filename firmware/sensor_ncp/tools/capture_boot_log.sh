#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h:h}"
PORT="${1:-/dev/cu.usbmodemLS41069U4}"
SECONDS_TO_CAPTURE="${2:-30}"
BAUD="${SWO_BAUD:-3000000}"
STAMP="$(date +%Y-%m-%d_%H-%M-%S)"
LOG_PATH="${ROOT_DIR}/logs/swo_boot_${STAMP}.log"

mkdir -p "${ROOT_DIR}/logs"
cd "${ROOT_DIR}"

python3 tools/capture_itm_text.py "${PORT}" "${BAUD}" --seconds "${SECONDS_TO_CAPTURE}" | tee "${LOG_PATH}" &
CAPTURE_PID=$!

cleanup() {
  kill "${CAPTURE_PID}" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

sleep 1
make flash
wait "${CAPTURE_PID}"

printf "Saved SWO boot log to %s\n" "${LOG_PATH}"
