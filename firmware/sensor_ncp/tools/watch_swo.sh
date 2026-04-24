#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h:h}"
LOG_DIR="${ROOT_DIR}/logs/swo"
DSS_BIN="/Applications/ti/uniflash_9.5.0/deskdb/content/TICloudAgent/osx/ccs_base/scripting/bin/dss.sh"
PROBE_SCRIPT="${ROOT_DIR}/tools/enable_swo_probe.js"
HOLD_MS="${SWO_HOLD_MS:-86400000}"
BAUD="${SWO_BAUD:-3000000}"
PORT_GLOB="${SWO_PORT_GLOB:-/dev/cu.usbmodem*}"
PORT=""
LOGGER_PID=""
TAIL_PID=""

pick_x86_java() {
  local candidate java_arch
  local -a candidates

  if [[ -n "${SWO_DSS_JAVA:-}" ]]; then
    candidates+=("${SWO_DSS_JAVA}")
  fi

  candidates+=(
    /Applications/ti/xdctools_*/jre/bin/java
    /usr/bin/java
  )

  for candidate in ${~candidates}; do
    [[ -x "${candidate}" ]] || continue
    if java_arch="$(arch -x86_64 "${candidate}" -XshowSettings:properties -version 2>&1 | awk '/os.arch =/ {print $3; exit}')" \
      && [[ "${java_arch}" == "x86_64" ]]; then
      printf "%s\n" "${candidate}"
      return 0
    fi
  done

  return 1
}

pick_port() {
  local candidate
  local -a matches

  if [[ -n "${PORT}" ]]; then
    printf "%s\n" "${PORT}"
    return 0
  fi

  matches=(${~PORT_GLOB}(N))
  if (( ${#matches[@]} == 0 )); then
    return 1
  fi

  candidate="${matches[-1]}"
  printf "%s\n" "${candidate}"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="${2:?missing value for --port}"
      shift 2
      ;;
    --port=*)
      PORT="${1#*=}"
      shift
      ;;
    --port-glob)
      PORT_GLOB="${2:?missing value for --port-glob}"
      shift 2
      ;;
    --port-glob=*)
      PORT_GLOB="${1#*=}"
      shift
      ;;
    *)
      PORT="$1"
      shift
      ;;
  esac
done

mkdir -p "${LOG_DIR}"
cd "${ROOT_DIR}"

JAVA_BIN="$(pick_x86_java || true)"
if [[ -z "${JAVA_BIN}" ]]; then
  echo "Passive SWO watch is unavailable: no x86_64 Java runtime was found for TI DSS." >&2
  echo "Set SWO_DSS_JAVA=/path/to/x86_64/java or install an x86_64 JRE. The repo can use /Applications/ti/xdctools_*/jre/bin/java when present." >&2
  exit 1
fi

JAVA_HOME="${JAVA_BIN:A:h:h}"
export JAVA_HOME
export PATH="${JAVA_HOME}/bin:${PATH}"

if [[ -n "${PORT}" ]]; then
  echo "Watching SWO on ${PORT} at ${BAUD} baud"
else
  DETECTED_PORT="$(pick_port || true)"
  if [[ -n "${DETECTED_PORT}" ]]; then
    echo "Watching SWO on ${DETECTED_PORT} at ${BAUD} baud"
  else
    echo "Watching SWO with port auto-detect (${PORT_GLOB}) at ${BAUD} baud"
  fi
fi
echo "Using DSS Java ${JAVA_BIN}"
echo "Probe session log ${LOG_DIR}/probe_session.log"

arch -x86_64 bash "${DSS_BIN}" "${PROBE_SCRIPT}" "${HOLD_MS}" > "${LOG_DIR}/probe_session.log" 2>&1 &
PROBE_PID=$!

cleanup() {
  if [[ -n "${TAIL_PID}" ]]; then
    kill "${TAIL_PID}" >/dev/null 2>&1 || true
  fi
  if [[ -n "${LOGGER_PID}" ]]; then
    kill "${LOGGER_PID}" >/dev/null 2>&1 || true
  fi
  kill "${PROBE_PID}" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

sleep 2
if ! kill -0 "${PROBE_PID}" >/dev/null 2>&1; then
  echo "DSS probe session exited early. Check ${LOG_DIR}/probe_session.log for the failure details." >&2
fi

CURRENT_LOG="${LOG_DIR}/swo_$(date +%Y-%m-%d).log"
touch "${CURRENT_LOG}"
echo "Streaming ${CURRENT_LOG}"

if [[ -n "${PORT}" ]]; then
  python3 tools/swo_log_daemon.py --quiet-stdout --port "${PORT}" --baud "${BAUD}" --log-dir "${LOG_DIR}" &
else
  python3 tools/swo_log_daemon.py --quiet-stdout --port-glob "${PORT_GLOB}" --baud "${BAUD}" --log-dir "${LOG_DIR}" &
fi
LOGGER_PID=$!

tail -n 0 -F "${CURRENT_LOG}" &
TAIL_PID=$!
wait "${TAIL_PID}"
