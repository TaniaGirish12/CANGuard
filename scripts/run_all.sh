#!/usr/bin/env bash
# Runs canguard on every attack file (*.csv) in data/raw and collects the metrics.
#
# Usage: scripts/run_all.sh [canguard options]
# Options are passed on to canguard. Without --train or --train-from-input the script uses
# --train-from-input 60. To train on the attack-free recording instead:
#   scripts/run_all.sh --train data/raw/normal_run_data.txt
#
# Each file gets its own folder results/<name>/ (metrics.csv, timings.csv, alerts.csv), and
# results/summary.csv holds one line per file.
#
# Environment variables:
#   CANGUARD_BIN          binary to run          (default: build/canguard, built if missing)
#   CANGUARD_INPUT_DIR    folder with *.csv files (default: data/raw)
#   CANGUARD_RESULTS_DIR  output folder           (default: results)

set -euo pipefail
cd "$(dirname "$0")/.."

BIN="${CANGUARD_BIN:-build/canguard}"
INPUT_DIR="${CANGUARD_INPUT_DIR:-data/raw}"
RESULTS_DIR="${CANGUARD_RESULTS_DIR:-results}"

if [[ ! -x "$BIN" ]]; then
    echo "Building $BIN ..."
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release > /dev/null
    cmake --build build -j > /dev/null
fi

shopt -s nullglob
files=("$INPUT_DIR"/*.csv)
if [[ ${#files[@]} -eq 0 ]]; then
    echo "No .csv files in $INPUT_DIR. Put the dataset there (see README) or set CANGUARD_INPUT_DIR." >&2
    exit 1
fi

has_training_option=false
for argument in "$@"; do
    if [[ "$argument" == "--train" || "$argument" == "--train-from-input" ]]; then
        has_training_option=true
    fi
done

metric() {  # metric <metrics.csv> <name>
    awk -F, -v name="$2" '$1 == name { print $2 }' "$1"
}

mkdir -p "$RESULTS_DIR"
summary="$RESULTS_DIR/summary.csv"
echo "file,precision,recall,f1,false_positive_rate,true_positives,false_positives,true_negatives,false_negatives" > "$summary"

for file in "${files[@]}"; do
    name="$(basename "$file" .csv)"
    out="$RESULTS_DIR/$name"
    echo "=== $file"

    if $has_training_option; then
        "$BIN" --input "$file" --out "$out" "$@"
    else
        "$BIN" --input "$file" --out "$out" --train-from-input 60 "$@"
    fi
    echo

    metrics="$out/metrics.csv"
    echo "$name,$(metric "$metrics" precision),$(metric "$metrics" recall),$(metric "$metrics" f1),$(metric "$metrics" false_positive_rate),$(metric "$metrics" true_positives),$(metric "$metrics" false_positives),$(metric "$metrics" true_negatives),$(metric "$metrics" false_negatives)" >> "$summary"
done

echo "=== Summary ($summary)"
column -s, -t "$summary"
