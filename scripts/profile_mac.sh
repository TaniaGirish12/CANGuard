#!/usr/bin/env bash
# Profiles canguard with the Instruments "Time Profiler" template (macOS only).
#
# Usage: scripts/profile_mac.sh <canguard arguments>
# Example: scripts/profile_mac.sh --train-from-input 60 --input data/raw/DoS_dataset.csv
#
# Builds a Release binary with debug symbols in build-prof/, records the run to
# results/canguard.trace and tells you how to open it.

set -euo pipefail

if [[ $# -eq 0 ]]; then
    sed -n '2,6p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
fi

if [[ "$(uname)" != "Darwin" ]]; then
    echo "profile_mac.sh needs macOS and Instruments. On Linux use: perf record -g ./build/canguard ARGS" >&2
    exit 1
fi

if ! xcrun --find xctrace > /dev/null 2>&1; then
    echo "xctrace not found. It comes with the full Xcode app, not the Command Line Tools." >&2
    echo "Install Xcode, then run: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
    exit 1
fi

cd "$(dirname "$0")/.."

cmake -S . -B build-prof -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-g
cmake --build build-prof -j
dsymutil build-prof/canguard

mkdir -p results
rm -rf results/canguard.trace

xcrun xctrace record --template 'Time Profiler' --output results/canguard.trace --launch -- ./build-prof/canguard "$@"

echo
echo "Trace written to results/canguard.trace"
echo "Open it with:  open results/canguard.trace"
