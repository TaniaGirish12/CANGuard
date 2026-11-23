# CANGuard

A C++20 command-line program that detects injected (attack) messages in recorded CAN bus traffic.
It learns what normal traffic looks like for every CAN ID, flags messages that break those
expectations, and compares its decisions with the ground-truth labels in the data.

Phase 1 (sequential pipeline, thread-safe `BoundedBuffer`, tests, per-stage timing, profiling script)
is implemented. Phase 2 (parallel pipelines) has interface headers only.

- [How it works](#how-it-works)
- [Requirements](#requirements)
- [Dataset](#dataset)
- [Build](#build)
- [Run](#run)
- [Synthetic data with `cangen`](#synthetic-data-with-cangen)
- [Test](#test)
- [Run on every attack file](#run-on-every-attack-file)
- [Profile](#profile)
- [Profile findings and parallel design proposal](#profile-findings-and-parallel-design-proposal)
- [Results and known limitations](#results-and-known-limitations)
- [Project layout](#project-layout)

## How it works

**Training.** CANGuard builds one profile per CAN ID from normal traffic: the known-ID set, the mean and
standard deviation of the time between messages of that ID, the min, max and constancy of every data byte plus the largest change of each byte between consecutive
messages, the usual DLC, and the message count (which gives the expected rate). Normal traffic comes from an
attack-free file (`--train`) or from the R-flagged messages in the first N seconds of the input
(`--train-from-input N`).

**Detection.** Every message is checked, in timestamp order, by four rules:

| Rule | Fires when |
|---|---|
| `UnknownId` | the ID is not in the known-ID set |
| `Timing` | inter-arrival < max(mean - k * std, mean * r); needs at least 2 earlier messages of that ID |
| `Rate` | messages of this ID in the last window (default 100 ms) > expected count * rate-factor |
| `Payload` | the DLC differs from the usual one, or a data byte both jumps (changes by more than twice the largest change between two consecutive messages in training, plus 4) and lands outside its trained [min, max] |

The `Payload` rule deliberately differs from the plain "byte outside [min, max]" test: on real recordings that
test flags 9% to 35% of normal messages, because sensor values keep drifting outside whatever range the training
window covered. Requiring a jump as well tolerates drift and one-off state changes but still catches an injected
payload. See [Results and known limitations](#results-and-known-limitations).

A message is classified as an attack when at least `--min-score` rules fire (default 1), and the rules that
fired are recorded. The expected count in the `Rate` rule is never lower than 1; without that floor, every
normal message of a slow ID (one per second, say) would trip the rule.

**Evaluation.** Decisions are compared with the `Flag` column (`T` = injected = positive class): confusion
matrix, precision, recall, F1, false positive rate, and per-rule fire counts with true and false positives.

## Requirements

- CMake 3.20 or newer
- A C++20 compiler: Apple clang (primary target, developed and tested on macOS), or GCC 11+ / clang on Linux
- No external libraries: the standard library and system threads only

The Linux toolchains have not been tried yet. The code uses only standard C++20 (plus `unistd.h` in the
tests), so it should build there; the build is expected to be free of warnings under `-Wall -Wextra`.

## Dataset

CANGuard is written for the **HCRL Car-Hacking Dataset**:
<https://sites.google.com/hksecurity.net/hcrl/Datasets/car-hacking-dataset>. See that page for the terms of
use and how to cite it.

1. Download the dataset from the page above. It contains `DoS_dataset.csv`, `Fuzzy_dataset.csv`,
   `gear_dataset.csv`, `RPM_dataset.csv` and the attack-free recording (`normal_run_data.7z`, which unpacks to
   `normal_run_data.txt`).
2. Put those files into `data/raw/`. The folder is ignored by git, so the large files are never committed:

   ```bash
   mkdir -p data/raw
   cp "path/to/Car-Hacking Dataset"/*.csv data/raw/
   cp "path/to/Car-Hacking Dataset"/normal_run_data/normal_run_data.txt data/raw/
   ```

File names may vary; every path is passed on the command line. The two formats:

- **Attack files** are CSV with no header: `Timestamp,CAN_ID,DLC,DATA[0],...,DATA[DLC-1],Flag`. The timestamp is
  in seconds, the ID and data bytes are hex, and the flag is `T` (injected) or `R` (normal). With a DLC below 8
  the line has fewer data fields, so the flag is always the last field. The real files use Windows (`\r\n`) line
  endings, which the parser handles.
- **The attack-free file** is text, one message per line:
  `Timestamp: 1479121434.850202        ID: 0350    000    DLC: 8    05 28 84 66 6d 00 00 a2`. Every message
  in it is normal.

Malformed lines are skipped and counted, and the count is reported (`skipped=N`). The real attack-free file
ends in a truncated line, so one skipped line there is expected.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

| Build type | Flags |
|---|---|
| `Release` (default) | `-O2` |
| `Debug` | `-O0 -g` |

Add `-DCANGUARD_TSAN=ON` to build every target with ThreadSanitizer (use a separate folder, for example
`-B build-tsan`).

Targets: `canguard` (the detector), `cangen` (synthetic data generator) and `canguard_tests`. All three link the
static library `canguard_core`.

## Run

```bash
# Train on the attack-free recording, analyse one attack file
build/canguard --train data/raw/normal_run_data.txt --input data/raw/DoS_dataset.csv

# Or learn from the first 60 seconds of the input itself (R-flagged messages only)
build/canguard --train-from-input 60 --input data/raw/RPM_dataset.csv --out results/rpm
```

Exactly one of `--train` or `--train-from-input` is required.

| Flag | Meaning | Default |
|---|---|---|
| `--train PATH` | Normal traffic file for training | none |
| `--train-from-input SEC` | Train on R-flagged messages from the first SEC seconds of the input | off |
| `--input PATH` | File to analyse | required |
| `--mode NAME` | `sequential` (Phase 2 will add `idpart`, `timewin`, `staged`) | `sequential` |
| `--threads N` | Worker threads (used by Phase 2 modes) | hardware concurrency |
| `--k`, `--r`, `--window-ms`, `--rate-factor`, `--min-score` | Rule settings | 3, 0.2, 100, 3, 1 |
| `--out DIR` | Output folder | `results` |
| `--repeat N` | Run N times, report mean and standard deviation of the stage timings | 1 |

Example output:

```text
CANGuard | mode=sequential | input=DoS_dataset.csv | frames=3665771 | skipped=0
Stage          time (ms)    share
read+parse         390.8    80.9%
train                0.7     0.1%
detect              75.8    15.7%
evaluate            15.6     3.2%
Precision 0.997  Recall 1.000  F1 0.999  FPR 0.001
Rules fired: Timing 967  Rate 58  Payload 630  UnknownId 587521
```

With `--repeat 3` the table gains a standard-deviation column. If the training file had malformed lines, the
header adds `(+N in training file)`.

Each run writes three files into the output folder:

- `metrics.csv`: `metric,value` rows with the confusion matrix, precision, recall, F1, false positive rate and
  per-rule fired / true positive / false positive counts.
- `timings.csv`: `stage,mean_ms,std_ms,share_percent` for `read+parse`, `train`, `detect` and `evaluate`.
- `alerts.csv`: one row per message classified as an attack: `timestamp,id,rules_fired,true_label`, with the
  rules joined by `|` and the true label as `T` or `R`. This file can be large (about 20 MB for the DoS file
  with `--train-from-input 60`).

Exit codes: 0 success, 1 runtime error (for example an unreadable file), 2 bad command line.

## Synthetic data with `cangen`

`cangen` writes attack-format CSV with several periodic IDs and stable payloads, plus attack bursts flagged `T`.
The same seed always gives the same file.

```bash
build/cangen --out data/synthetic/synthetic_dos.csv --seconds 60 --attack dos --seed 42
build/canguard --train-from-input 5 --input data/synthetic/synthetic_dos.csv
```

`--attack` is `dos`, `fuzzy`, `spoof` or `none`. Bursts are 3 s long, every 10 s, and the first one starts at
10 s so the start of the file is clean training material. A run shorter than 10 s therefore contains no attack.
Four 60-second samples (seed 42) are in `data/synthetic/`.

## Test

```bash
ctest --test-dir build --output-on-failure
```

The tests use a small self-made runner (no framework): parser, rules, detector, profile builder, evaluator,
stage timer, result writer, CLI, console report, `BoundedBuffer` (blocking behaviour, and 4 producers and
4 consumers moving 100,000 items exactly once), the synthetic generator and end-to-end runs (a synthetic DoS file
trained with `--train-from-input` must reach a recall of at least 0.9).

The threaded tests must also pass under ThreadSanitizer:

```bash
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Release -DCANGUARD_TSAN=ON
cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure
```

## Run on every attack file

```bash
scripts/run_all.sh
```

Runs `canguard` on every `*.csv` in `data/raw`, writes each file's results to `results/<name>/`, and collects
one line per file in `results/summary.csv`. Without a training option it uses `--train-from-input 60`; any
options you give are passed on to `canguard`:

```bash
scripts/run_all.sh --train data/raw/normal_run_data.txt
scripts/run_all.sh --min-score 2 --repeat 3
```

`CANGUARD_BIN`, `CANGUARD_INPUT_DIR` and `CANGUARD_RESULTS_DIR` change the binary, the input folder and the
output folder (for example `CANGUARD_INPUT_DIR=data/synthetic scripts/run_all.sh`).

## Profile

**Per-stage timing.** Every run reports the wall-clock time of each stage on the console and in
`results/timings.csv`. Use `--repeat N` for a mean and standard deviation; the first repeat is usually the
slowest because the file is not yet in the operating system's cache.

**macOS, Instruments.** This needs the full Xcode app (not only the Command Line Tools), because `xctrace` is
part of Xcode.

```bash
scripts/profile_mac.sh --train-from-input 60 --input data/raw/DoS_dataset.csv
```

The script builds a Release binary with debug symbols in `build-prof/`, runs
`xcrun xctrace record --template 'Time Profiler' --output results/canguard.trace --launch -- ./build-prof/canguard ARGS`
with the arguments you gave it, and writes `results/canguard.trace`. Open the trace in Instruments with

```bash
open results/canguard.trace
```

(or File > Open in Instruments). Select the **Time Profiler** track and use the Call Tree view; "Invert Call
Tree" and "Hide System Libraries" in the Call Tree options put the heaviest functions at the top.

**macOS without Xcode, `sample`.** The Command Line Tools include `sample`, a sampling profiler that needs no
Instruments:

```bash
scripts/profile_sample.sh --train-from-input 60 --input data/raw/RPM_dataset.csv
```

A single run is too short to sample, so the script runs the program 14 times in a row, samples it for five seconds
and prints the hottest functions as a share of all samples (full report in `results/sample.txt`).

**Linux, perf.**

```bash
cmake -S . -B build-prof -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-g
cmake --build build-prof -j
perf record -g ./build-prof/canguard --train-from-input 60 --input data/raw/DoS_dataset.csv
perf report
```

## Profile findings and parallel design proposal

The end goal is a parallel design, so Phase 1 ends with a profile of the sequential program and a proposal built on
it. **Nothing below is a measured speed-up: no parallel code exists yet. The bounds are calculated from the
sequential profile.**

### Profile of the sequential version

`scripts/profile_sample.sh` on the RPM file (4.6 million messages): 4,266 one-millisecond samples, all on the main
thread, peak memory about 0.5 GB. A second independent run gave the same picture, with shares within about two points.

| Part | Share of samples | Where |
|---|---|---|
| Parsing lines | 75% | hex data bytes 27%, `memchr` searching for the next separator 23%, timestamps 13%, line and field handling 12% |
| Reading the file | 5% | `read` |
| Detection | 16% | `PayloadRule` 4%, `RateRule` 4%, `Detector::process` 4%, per-ID state lookup 2%, `TimingRule` and `UnknownIdRule` under 1% |
| Evaluation | 3% | `Evaluator::add`, collecting alerts |
| Training | under 1% | `ProfileBuilder::add` |

This agrees with the stage timer (`read+parse` about 80%, `detect` 15% to 17%, `evaluate` 3%).

What it says:
- **Parsing is the bottleneck.** Every line is independent of the others, so it is the first thing to parallelise.
- **A sequential improvement is available first.** About a quarter of all samples are in `memchr`, called once per
  field. A single-pass tokenizer would cut that. It has not been done.
- **Detection is only about 16%, and it cannot be split arbitrarily.** A message needs the earlier messages of its own
  CAN ID, in order. All detection state lives in one `IdState` per ID.

### Proposed decomposition

| Stage | Technique | Why |
|---|---|---|
| Parse (75%) | Data decomposition into line-aligned time chunks, parsed by worker threads (`TimeWindowPipeline`, `readChunk` already exists) | lines are independent |
| Detect (16%) | Data decomposition by CAN ID: every ID is owned by one worker (`IdPartitionedPipeline`) | state is per ID, so no locks are needed between workers |
| Whole run | Pipeline decomposition: reader, parser workers, reorder step, detector workers, alert collector, connected by bounded buffers (`StagedPipeline`) | overlaps the stages |

**Upper bounds from Amdahl's law**, using the shares above (before any overhead):

| Workers | Only parsing parallel | Parsing and detection parallel |
|---|---|---|
| 2 | 1.6x | 1.8x |
| 4 | 2.3x | 3.1x |
| 8 | 2.9x | 4.8x |
| 10 | 3.1x | 5.4x |
| unlimited | 4.0x | 10.6x |

**Load balance decides whether the ID split works.** A plain `id mod N` assignment is poor, because CAN IDs cluster
(many are multiples of 16). Upper bound on the detection speed-up on the real files (computed from each file's own
message counts, so these are best cases):

| File (busiest ID) | Workers | `id mod N` | Mixing hash | Greedy by message count | Ideal |
|---|---|---|---|---|---|
| DoS (ID `0000`, 16.0%) | 4 | 1.70x | 2.89x | 3.99x | 4x |
| DoS | 8 | 1.70x | 3.36x | 6.24x | 8x |
| RPM (ID `0316`, 18.9%) | 4 | 2.29x | 2.67x | 3.93x | 4x |
| RPM | 8 | 2.29x | 3.87x | 5.30x | 8x |

The busiest single ID caps any ID-based split (6.2x on DoS, 5.3x on RPM). The plan is to assign IDs greedily by the
message counts that `IdProfile` already stores from training, and to send IDs unseen in training (such as the DoS ID
`0000`) to a worker chosen by a mixing hash. The real balance will be somewhat worse than the table, because the
attack IDs are not in the training counts.

### Synchronisation

| Where | Mechanism | Effect it handles |
|---|---|---|
| Hand-over between stages (`BoundedBuffer`) | Counting semaphores for free and filled slots, plus a mutex around the queue | Back-pressure, so a fast reader cannot flood memory; blocking without spinning; `close()` passes the wake-up on, so no thread is left waiting at shutdown. Built and tested |
| Task queue (`ThreadPool`) | Mutex and condition variable | Workers sleep while the queue is empty. Planned |
| Ordered alerts (`AlertManager`) | Condition variable waiting for the next chunk number | Chunks finish in any order, alerts must come out in input order. Planned |

### Chunk borders (time-window decomposition)

The state the rules need is small: the previous timestamp and payload of each ID, the message count, and the last
100 ms of timestamps. A warm-up overlap of 100 ms covers the `Rate` window, but a few IDs send only about once per
second, and the `Timing` rule needs two earlier messages, so a 100 ms overlap could change some decisions at chunk
borders. The options are a longer overlap (a few seconds) or passing each ID's state from one chunk to the next in
order. Which one to use is to be decided in Phase 2 by comparing against the sequential results.

### How the parallel design will be assessed

- **Quantitative:** run time and throughput (messages per second) for 1, 2, 4 and 8 workers, speed-up (T1 / Tp) and
  efficiency (speed-up / workers), each repeated with `--repeat` for mean and standard deviation, and compared with
  the bounds above. Correctness: identical metrics to the sequential run on all four files, and clean under
  ThreadSanitizer.
- **Qualitative:** complexity of the border handling, whether the output order is deterministic, load balance across
  workers, memory use, and how the design degrades when one ID dominates (as in DoS).

## Results and known limitations

Default settings, on the four real attack files, trained two ways (precision / recall / F1):

| File | `--train-from-input 60` | `--train data/raw/normal_run_data.txt` |
|---|---|---|
| `DoS_dataset.csv` | 0.997 / 1.000 / 0.999 | 0.934 / 1.000 / 0.966 |
| `Fuzzy_dataset.csv` | 0.994 / 1.000 / 0.997 | 0.946 / 1.000 / 0.972 |
| `gear_dataset.csv` | 0.893 / 0.989 / 0.939 | 0.857 / 0.994 / 0.920 |
| `RPM_dataset.csv` | 0.891 / 0.998 / 0.942 | 0.851 / 0.999 / 0.919 |

Which rule catches what (with `--train-from-input 60`): for DoS, `UnknownId` catches every attack message (ID
`0000` never occurs in normal traffic); for fuzzy it catches 98% and `Payload` and `Timing` most of the rest; for
gear and RPM spoofing `Rate` catches 97% to 99% of the injected messages, `Timing` about 87% to 90% and `Payload`
about 12%.

**Why the payload rule is not the plain range test.** The first version tested every data byte against its trained
[min, max], as the method description says. It never missed an injected message, but flagged 9% (DoS) to 35%
(RPM) of the normal ones, giving a precision of only 0.32 to 0.67 with `--train-from-input 60` and 0.23 to 0.29
with a separate attack-free file. The false positives came from sensor bytes that drift beyond the trained range
(for example an ID whose byte sat at `83-84` in the first minute and moved by up to 23 later) and from bytes that
were constant while the car idled and changed once it drove. Fixed margins on the range did not solve it: even a
margin of 32 left a precision of 0.81 on RPM and 0.46 on the separate file, and margins large enough to help began
missing spoofed payloads. Testing for a jump instead of an absolute value did (see `PayloadRule`). The constants
(factor 2, slack 4) were chosen by trying a few settings on these same four files; results barely changed between
neighbouring settings, but they have not been validated on other data.

**What is left.**
- For gear and RPM spoofing, about 70,000 to 115,000 normal messages are still flagged. They are the real messages
  of the spoofed ID sent during an attack burst: the `Timing` and `Rate` rules work per ID, so they cannot tell a
  real message from an injected one of the same ID, and the real message often follows an injected one by a
  fraction of a millisecond. Telling them apart needs a rule that looks at the payload together with the timing;
  that is not implemented.
- Recall on spoofing is 0.989 to 0.999 rather than 1.0: the first injected messages of a burst are not yet
  conspicuous by timing or rate.
- `--min-score 2` no longer pays off. It removes DoS and fuzzy detection (only `UnknownId` fires on those
  messages, so recall drops to 0.000 and 0.003) and costs recall on spoofing (gear 0.858, RPM 0.904).
- The thresholds are fixed per ID from a single training window, so a change in driving state that stays within the
  jump limits but leaves the learned inter-arrival statistics is still counted as normal.

A whole run over a file of about 3.7 million messages takes roughly a second, and reading and parsing is
about 80% of that.

## Project layout

```text
CMakeLists.txt
include/canguard/   one header per class
src/                sources, main.cpp (canguard), cangen_main.cpp (cangen)
tests/              test runner and tests
scripts/            run_all.sh, profile_mac.sh, profile_sample.sh
docs/               class_diagram.md (Mermaid class diagrams)
data/raw/           real dataset (not committed)
data/synthetic/     small generated files
results/            output CSVs and traces (not committed)
```

**Phase 2 (headers only).** `ThreadPool`, `IdPartitionedPipeline`, `TimeWindowPipeline`, `StagedPipeline` and
`AlertManager` are declared in `include/canguard/` with their planned design described in comments; they have no
implementation yet. `SequentialPipeline` already implements the common `IPipeline` interface they will share.
See [docs/class_diagram.md](docs/class_diagram.md) for the class diagrams.
