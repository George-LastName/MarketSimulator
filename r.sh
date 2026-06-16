#!/usr/bin/env bash

set -euo pipefail

# Resolve project dir to this script's location so it works from any cwd.
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

# Defaults
ASAN=OFF
TSAN=OFF
TIDY=OFF
IWYU=OFF
DO_CLEAN=0
LOCAL=0
RUN_EXCHANGE=0
RUN_TRADER=0

usage() {
    cat <<EOF
Usage: ./r.sh [options]

Configures CMake with the chosen flags, builds, then optionally runs.

CMake flag toggles:
  -a    Enable AddressSanitizer  (-DENABLE_ASAN=ON)
  -t    Enable ThreadSanitizer   (-DENABLE_TSAN=ON)
  -y    Enable clang-tidy        (-DENABLE_CLANG_TIDY=ON)
  -i    Enable include-what-you-use (-DENABLE_IWYU=ON)
  -l    Enable LOCAL_TESTING mode (-DLOCAL_TESTING=ON)

Build:
  -x    Clean build directory

Run after build:
  -e    Run the Exchange executable
  -r    Run the Trader executable
  -h    Show this help

Examples:
  ./r.sh                # configure + build with defaults
  ./r.sh -x -a -e       # clean, build with ASAN, run Exchange
  ./r.sh -l             # build with LOCAL_TESTING enabled
  ./r.sh -e             # build, then run Exchange
EOF
}

while getopts ":atyilxerh" opt; do
    case "$opt" in
        a) ASAN=ON ;;
        t) TSAN=ON ;;
        y) TIDY=ON ;;
        i) IWYU=ON ;;
        l) LOCAL=1 ;;
        x) DO_CLEAN=1 ;;
        e) RUN_EXCHANGE=1 ;;
        r) RUN_TRADER=1 ;;
        h) usage; exit 0 ;;
        \?) echo "Unknown option: -$OPTARG" >&2; usage; exit 1 ;;
        :)  echo "Option -$OPTARG requires an argument." >&2; exit 1 ;;
    esac
done

if [[ "$ASAN" == ON && "$TSAN" == ON ]]; then
    echo "Error: ASAN and TSAN cannot both be enabled (CMake will fatal-error)." >&2
    exit 1
fi

if [[ "$TIDY" == ON && "$IWYU" == ON ]]; then
    echo "Error: Clang-tidy and iwyu should not be enabled at the same time." >&2
    exit 1
fi

if (( DO_CLEAN )); then
    echo "==> Cleaning $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

LOCAL_FLAG=""
(( LOCAL )) && LOCAL_FLAG="-DLOCAL_TESTING=ON"

echo "==> Configuring (ASAN=$ASAN TSAN=$TSAN CLANG_TIDY=$TIDY IWYU=$IWYU LOCAL=$LOCAL)"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DENABLE_ASAN="$ASAN" \
    -DENABLE_TSAN="$TSAN" \
    -DENABLE_CLANG_TIDY="$TIDY" \
    -DENABLE_IWYU="$IWYU" \
    ${LOCAL_FLAG:+"$LOCAL_FLAG"}

# Determine if output should be captured to a report file
REPORT_FILE=""
[[ "$TIDY" == ON ]] && REPORT_FILE="$PROJECT_DIR/clang_tidy_report.txt"
[[ "$IWYU" == ON ]]  && REPORT_FILE="$PROJECT_DIR/iwyu_report.txt"

echo "==> Building"
if [[ -n "$REPORT_FILE" ]]; then
    echo "    Capturing output to $REPORT_FILE"
    cmake --build "$BUILD_DIR" 2>&1 | tee "$REPORT_FILE"
else
    cmake --build "$BUILD_DIR"
fi

if (( RUN_EXCHANGE )); then
    echo "==> Running Exchange"
    "$BUILD_DIR/exchange/Nasdaq_Exchange"
fi

if (( RUN_TRADER )); then
    echo "==> Running Trader"
    "$BUILD_DIR/trader/Nasdaq_Trader"
fi

echo "==> Done."
