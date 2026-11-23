#!/usr/bin/env bash
# Function-level CPU profile of canguard with macOS's `sample` tool. Works with the Command Line
# Tools alone (no Xcode needed, unlike profile_mac.sh).
#
# Usage: scripts/profile_sample.sh <canguard arguments>
# Example: scripts/profile_sample.sh --train-from-input 60 --input data/raw/RPM_dataset.csv
#
# A single run takes about half a second, too short to sample, so the script runs canguard 14 times
# in a row (--repeat 14), samples it for 5 seconds and prints the hottest functions as a share of all
# samples. The full report is written to results/sample.txt.

set -euo pipefail

if [[ $# -eq 0 ]]; then
    sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
fi

if [[ "$(uname)" != "Darwin" ]]; then
    echo "profile_sample.sh needs macOS. On Linux use: perf record -g ./build-prof/canguard ARGS" >&2
    exit 1
fi

cd "$(dirname "$0")/.."

cmake -S . -B build-prof -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-g > /dev/null
cmake --build build-prof -j > /dev/null
mkdir -p results

./build-prof/canguard "$@" --repeat 14 --out results/profile_run > results/profile_run.txt 2>&1 &
program=$!
sleep 1.5
if ! sample "$program" 5 1 -file results/sample.txt > /dev/null 2>&1; then
    echo "Could not sample the program. It may have finished too early: use a larger input file." >&2
    wait "$program" || true
    exit 1
fi
wait "$program"

echo "Hottest functions (share of all samples, one sample per millisecond):"
sed -n '/^Sort by top of stack/,/^Binary Images:/p' results/sample.txt \
    | sed -nE 's/^ +(.*)  \(in [^)]*\) +([0-9]+)$/\2 \1/p' \
    | sort -rn \
    | awk '{ count[NR] = $1; $1 = ""; label[NR] = substr($0, 2, 100); total += count[NR] }
           END { for (i = 1; i <= NR && i <= 15; i++) printf "%5.1f%%  %s\n", 100 * count[i] / total, label[i] }'
echo
echo "Full report: results/sample.txt   Program output: results/profile_run.txt"
