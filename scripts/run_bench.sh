#!/usr/bin/env bash
# Run ab benchmarks across thread counts and plot results.
set -euo pipefail

THREADS=${THREADS:-"1 2 4 8 16"}
N=${1:-1000}
C=${2:-50}
PORT=${PORT:-8080}
ROOT=${ROOT:-www}
BINARY=${BINARY:-./webserver}
OUT_DIR=${OUT_DIR:-bench_out}
NO_LRU=${NO_LRU:-0}
AUTO=${AUTO:-1}

DEFAULT_URL="http://127.0.0.1:${PORT}/"
URL_INPUT=${3:-"${DEFAULT_URL}"}

# Parse additional args/flags for URL and cache mode.
for arg in "$@"; do
  case "${arg}" in
    --noLRU|--no-cache)
      NO_LRU=1
      AUTO=0
      ;;
    --cache-only)
      NO_LRU=0
      AUTO=0
      ;;
    --auto)
      AUTO=1
      ;;
    http://*|https://*)
      URL_INPUT="${arg}"
      ;;
  esac
done

TARGET_URL=${URL_INPUT}

mkdir -p "${OUT_DIR}"

# Build the server
make

results_tsv="${OUT_DIR}/results.tsv"
echo -e "threads\tn\tc\tcache\trps\ttime_per_req_ms\ttransfer_rate_kBps\tfailed_requests" > "${results_tsv}"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill -INT "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

run_suite() {
  local cache_flag="$1"  # on/off
  local cache_label="${cache_flag}"
  local no_lru_arg=()
  if [[ "${cache_flag}" == "off" ]]; then
    no_lru_arg=("--noLRU")
  fi

  for T in ${THREADS}; do
    echo "Running threads=${T} n=${N} c=${C} url=${TARGET_URL} cache=${cache_label}"
    SERVER_ARGS=("-p" "${PORT}" "-t" "${T}" "-r" "${ROOT}" "${no_lru_arg[@]}")
    "${BINARY}" "${SERVER_ARGS[@]}" > "${OUT_DIR}/server_${T}_${cache_label}.log" 2>&1 &
    SERVER_PID=$!
    sleep 1

    ab_log="${OUT_DIR}/ab_${T}_${cache_label}.log"
    if ! ab -n "${N}" -c "${C}" "${TARGET_URL}" > "${ab_log}" 2>&1; then
      echo "ab failed for threads=${T} cache=${cache_label}, see ${ab_log}" >&2
      cleanup
      continue
    fi

    cleanup
    SERVER_PID=""

    rps=$(grep "Requests per second" "${ab_log}" | awk '{print $4}')
    tpr=$(grep -m1 "Time per request" "${ab_log}" | awk '{print $4}')
    # ab prints: "Transfer rate: 12345.67 [Kbytes/sec] received"; the 3rd field is the number.
    xfer=$(grep "Transfer rate" "${ab_log}" | awk '{print $3}')
    failed=$(grep "Failed requests" "${ab_log}" | awk '{print $3}')

    echo -e "${T}\t${N}\t${C}\t${cache_label}\t${rps:-0}\t${tpr:-0}\t${xfer:-0}\t${failed:-0}" >> "${results_tsv}"
    echo "Finished threads=${T} cache=${cache_label}: rps=${rps:-0}, time_per_req_ms=${tpr:-0}, transfer_kBps=${xfer:-0}, failed=${failed:-0}"
    sleep 1
  done
}

if [[ ${AUTO} -eq 1 ]]; then
  run_suite on
  run_suite off
else
  if [[ ${NO_LRU} -eq 1 ]]; then
    run_suite off
  else
    run_suite on
  fi
fi

trap - EXIT

python3 scripts/plot_results.py "${results_tsv}" "${OUT_DIR}/results.json" "${OUT_DIR}" || echo "Plotting failed; check Python/matplotlib installation" >&2

echo "Results written to ${results_tsv} and ${OUT_DIR}/results.json"
echo "Plots (if generated) saved under ${OUT_DIR}"
