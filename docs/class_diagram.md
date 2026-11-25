# CANGuard class diagrams

All diagrams are [Mermaid](https://mermaid.js.org/) and render on GitHub and in most Markdown viewers.

1. [Overview](#overview-phase-1): the 16 main classes and how they connect (start here).
2. [Phase 2 plan](#phase-2-headers-only-not-implemented-yet): the parallel classes that are declared but not built.
3. [Detailed Phase 1](#detailed-phase-1-every-class-and-struct): every class, struct and field from the specification.

## Overview (Phase 1)

One pass through the program: the pipeline reads the log, learns a profile per ID, runs every frame through the
detector and its four rules, scores the result and times each stage. The console report and the result writer
then present the run.

```mermaid
classDiagram
    direction TB

    class IPipeline {
        <<interface>>
        run(options) RunResult
    }
    class SequentialPipeline
    class RunResult {
        metrics, timings, alerts
    }

    class CanLogReader {
        reads and parses the log
    }
    class ProfileBuilder {
        learns normal per ID
    }
    class Detector {
        process(frame)
    }
    class IRule {
        <<interface>>
        evaluate(frame)
    }
    class UnknownIdRule
    class TimingRule
    class RateRule
    class PayloadRule

    class Evaluator {
        precision, recall, F1
    }
    class StageTimer {
        time per stage
    }
    class ConsoleReport
    class ResultWriter {
        metrics, timings, alerts as CSV
    }

    IPipeline <|.. SequentialPipeline
    SequentialPipeline --> CanLogReader : reads
    SequentialPipeline --> ProfileBuilder : learns
    SequentialPipeline --> Detector : detects
    SequentialPipeline --> Evaluator : scores
    SequentialPipeline --> StageTimer : times
    SequentialPipeline ..> RunResult : returns

    Detector o-- IRule : runs four
    IRule <|.. UnknownIdRule
    IRule <|.. TimingRule
    IRule <|.. RateRule
    IRule <|.. PayloadRule

    ConsoleReport ..> RunResult : prints
    ResultWriter ..> RunResult : saves

    %% ---- colours ----
    classDef api fill:#e8e6ff,stroke:#6c63c9,color:#24206b
    classDef input fill:#dbeafe,stroke:#3b82f6,color:#10315f
    classDef learn fill:#d7f5f1,stroke:#2ec4b6,color:#0b3b36
    classDef detect fill:#fff1cc,stroke:#f2a900,color:#5a3d00
    classDef output fill:#fde2e2,stroke:#e05252,color:#6b1414
    classDef sync fill:#dcfce7,stroke:#22a06b,color:#0f4a2e
    cssClass "IPipeline,SequentialPipeline,RunResult" api
    cssClass "CanLogReader" input
    cssClass "ProfileBuilder" learn
    cssClass "Detector,IRule,UnknownIdRule,TimingRule,RateRule,PayloadRule" detect
    cssClass "Evaluator,StageTimer,ConsoleReport,ResultWriter" output
```

Colours: purple = pipeline and results, blue = input, teal = learning, amber = detection, pink = scoring and output.

`BoundedBuffer<T>` is finished and tested but not used by the sequential pipeline; the Phase 2 pipelines build on it.

## Phase 2 (headers only, not implemented yet)

The three parallel designs are declared in `include/canguard/` but not built. They all keep the Phase 1
detector; what changes is how the work is split across threads.

### Parallel flow

Read it left to right: every design starts from `IPipeline` and ends in the same `RunResult`. The three lanes are
stacked one above the other, and each lane is one design (chosen with `--mode`). Boxes stacked on top of each other
inside a lane run at the same time. Amber
boxes are the synchronisation points: `BoundedBuffer` (counting semaphores and a mutex) between threads, a mutex
and condition variable inside `ThreadPool`, and a condition variable in `AlertManager`.

```mermaid
flowchart LR
    R["IPipeline<br/>run(options)"]
    RR["RunResult<br/>same alerts as the sequential run"]

    subgraph A["1. IdPartitionedPipeline (idpart): split by CAN ID"]
        A0["Dispatcher thread<br/>read and parse"]
        AB1[["BoundedBuffer 1"]]
        AB2[["BoundedBuffer 2"]]
        AB3[["BoundedBuffer N"]]
        AW1["Worker 1<br/>Detector for its IDs"]
        AW2["Worker 2<br/>Detector for its IDs"]
        AW3["Worker N<br/>Detector for its IDs"]
        AM["Merge alerts"]
        A0 --> AB1 --> AW1 --> AM
        A0 --> AB2 --> AW2 --> AM
        A0 --> AB3 --> AW3 --> AM
    end
    R --> A0
    AM --> RR

    subgraph B["2. TimeWindowPipeline (timewin): split by time chunk"]
        B0["Split input into<br/>time chunks"]
        BP[("ThreadPool<br/>mutex + condition variable")]
        BC1["Chunk 1<br/>parse and detect"]
        BC2["Chunk 2<br/>parse and detect"]
        BC3["Chunk N<br/>parse and detect"]
        BA["AlertManager<br/>condition variable restores input order"]
        B0 --> BP
        BP --> BC1 --> BA
        BP --> BC2 --> BA
        BP --> BC3 --> BA
    end
    R --> B0
    BA --> RR

    subgraph C["3. StagedPipeline (staged): split by stage"]
        C0["Reader thread<br/>raw chunks"]
        CB1[["BoundedBuffer"]]
        CP1["Parser 1"]
        CP2["Parser 2"]
        CP3["Parser M"]
        CB2[["BoundedBuffer"]]
        CR["Reorder by<br/>chunk number"]
        CD["Detector thread<br/>frames in order"]
        CA["AlertManager<br/>collects alerts"]
        C0 --> CB1
        CB1 --> CP1 --> CB2
        CB1 --> CP2 --> CB2
        CB1 --> CP3 --> CB2
        CB2 --> CR --> CD --> CA
    end
    R --> C0
    CA --> RR

    classDef thread fill:#d7f5f1,stroke:#2ec4b6,color:#0b3b36
    classDef sync fill:#fff1cc,stroke:#f2a900,color:#5a3d00
    classDef api fill:#e8e6ff,stroke:#6c63c9,color:#24206b
    class R,RR api
    class A0,AW1,AW2,AW3,AM,B0,BC1,BC2,BC3,C0,CP1,CP2,CP3,CR,CD thread
    class AB1,AB2,AB3,BP,BA,CB1,CB2,CA sync
```

What each design is for:

| Design | Splits the work by | Parallel part | Hard part |
|---|---|---|---|
| `idpart` | CAN ID | Detection (about 16% of the run) | Load balance: one ID can carry 16% to 19% of the traffic |
| `timewin` | Time chunk | Parsing and detection (about 91%) | State at chunk borders: warm-up overlap or hand-off between chunks |
| `staged` | Pipeline stage | Parsing (about 75%) | Keeping frames in order between the parsers and the detector |

### Class relationships

Teal = the parallel pipelines and the thread pool, green = synchronisation classes, purple = the shared interface.

```mermaid
classDiagram
    direction LR

    class IPipeline {
        <<interface>>
        +run(options) RunResult
    }
    class IdPartitionedPipeline {
        +run(options) RunResult
        +workerIndexFor(id, workerCount)$ size_t
    }
    class TimeWindowPipeline {
        +run(options) RunResult
    }
    class StagedPipeline {
        +run(options) RunResult
    }
    class ThreadPool {
        +submit(task) future
        +shutdown() void
        +workerCount() size_t
    }
    class AlertManager {
        +submit(chunkSequenceNumber, alerts) void
        +finish(chunkCount) vector~Alert~
    }
    class BoundedBuffer~T~ {
        +push(item) bool
        +pop() optional~T~
        +close() void
    }

    IPipeline <|.. IdPartitionedPipeline
    IPipeline <|.. TimeWindowPipeline
    IPipeline <|.. StagedPipeline
    IdPartitionedPipeline ..> BoundedBuffer : one per worker
    StagedPipeline ..> BoundedBuffer : between stages
    StagedPipeline ..> AlertManager : orders alerts
    TimeWindowPipeline ..> AlertManager : orders alerts
    TimeWindowPipeline ..> ThreadPool : parallel chunks

    %% ---- colours ----
    classDef api fill:#e8e6ff,stroke:#6c63c9,color:#24206b
    classDef input fill:#dbeafe,stroke:#3b82f6,color:#10315f
    classDef learn fill:#d7f5f1,stroke:#2ec4b6,color:#0b3b36
    classDef detect fill:#fff1cc,stroke:#f2a900,color:#5a3d00
    classDef output fill:#fde2e2,stroke:#e05252,color:#6b1414
    classDef sync fill:#dcfce7,stroke:#22a06b,color:#0f4a2e
    cssClass "IPipeline" api
    cssClass "IdPartitionedPipeline,TimeWindowPipeline,StagedPipeline,ThreadPool" learn
    cssClass "AlertManager,BoundedBuffer" sync
```

## Detailed Phase 1: every class and struct

Same colours as the overview, plus green for `BoundedBuffer` (synchronisation).

Every class and struct from the specification, plus the small supporting types they pass around
(`ReadResult`, `Detection`, `Metrics`, `RunResult` and so on).

```mermaid
classDiagram
    direction LR

    %% ---- input ----
    class CanFrame {
        <<struct>>
        +double timestamp
        +uint32 id
        +uint8 dlc
        +array data
        +bool isAttack
        +size_t lineNumber
    }
    class LogFormat {
        <<enumeration>>
        AttackCsv
        NormalText
    }
    class ReadResult {
        <<struct>>
        +vector~CanFrame~ frames
        +size_t skippedLines
    }
    class CanLogReader {
        +readAll() ReadResult
        +readChunk(firstLineNumber, lineCount) ReadResult
        +detectFormat(text)$ LogFormat
        +parseLine(line, format)$ optional~CanFrame~
        +parseText(text, format, firstLineNumber)$ ReadResult
    }
    CanLogReader ..> LogFormat : detects
    CanLogReader ..> ReadResult : returns
    ReadResult o-- CanFrame

    %% ---- learning ----
    class IdProfile {
        <<struct>>
        +uint32 id
        +size_t messageCount
        +double trainingSeconds
        +uint8 usualDlc
        +double interArrivalMean
        +double interArrivalStd
        +array minByte
        +array maxByte
        +array maxByteJump
        +isByteConstant(index) bool
        +expectedCountInWindow(windowSeconds) double
    }
    class ProfileBuilder {
        +add(frame) void
        +build() ProfileMap
    }

    ProfileBuilder ..> CanFrame : learns from
    ProfileBuilder ..> IdProfile : builds one per ID

    %% ---- detection ----
    class DetectorConfig {
        <<struct>>
        +double k
        +double r
        +double windowMs
        +double rateFactor
        +size_t minScore
    }
    class IdState {
        <<struct>>
        +double lastTimestamp
        +size_t previousMessages
        +deque~double~ recentTimestamps
        +array lastData
        +uint8 lastDlc
        +record(frame) void
    }
    class RuleResult {
        <<struct>>
        +bool fired
    }
    class IRule {
        <<interface>>
        +evaluate(frame, state, profile) RuleResult
        +name() string_view
    }
    class UnknownIdRule
    class TimingRule
    class RateRule
    class PayloadRule
    class Detection {
        <<struct>>
        +size_t index
        +bool predictedAttack
        +uint32 ruleMask
    }
    class Detector {
        +process(frame) Detection
        +ruleCount() size_t
        +ruleName(ruleIndex) string_view
    }

    IRule <|.. UnknownIdRule
    IRule <|.. TimingRule
    IRule <|.. RateRule
    IRule <|.. PayloadRule
    IRule ..> RuleResult : returns
    IRule ..> IdState : reads and prunes
    IRule ..> IdProfile : compares with
    TimingRule --> DetectorConfig : k and r
    RateRule --> DetectorConfig : window and rateFactor
    Detector *-- IRule : owns four
    Detector *-- IdState : one per CAN ID
    Detector --> IdProfile : looks up by ID
    Detector --> DetectorConfig : minScore
    Detector ..> Detection : returns

    %% ---- evaluation and output ----
    class RuleStats {
        <<struct>>
        +string name
        +size_t truePositives
        +size_t falsePositives
        +fired() size_t
    }
    class Metrics {
        <<struct>>
        +size_t truePositives
        +size_t falsePositives
        +size_t trueNegatives
        +size_t falseNegatives
        +double precision
        +double recall
        +double f1
        +double falsePositiveRate
    }
    class Evaluator {
        +add(detection, isAttack) void
        +metrics() Metrics
    }
    class StageTiming {
        <<struct>>
        +string name
        +double milliseconds
    }
    class StageSummary {
        <<struct>>
        +string name
        +double meanMilliseconds
        +double stdMilliseconds
        +double sharePercent
    }
    class StageTimer {
        +start(stage) void
        +stop(stage) void
        +measure(stage, work) void
        +timings() vector~StageTiming~
        +summarize(runs)$ vector~StageSummary~
    }
    class Alert {
        <<struct>>
        +double timestamp
        +uint32 id
        +uint32 ruleMask
        +bool isAttack
    }
    class ResultWriter {
        +writeMetrics(metrics) void
        +writeTimings(timings) void
        +writeAlerts(alerts) void
    }

    Metrics *-- RuleStats
    Evaluator ..> Detection : accumulates
    Evaluator ..> Metrics : computes
    StageTimer ..> StageTiming
    StageTimer ..> StageSummary
    ResultWriter ..> Metrics : metrics.csv
    ResultWriter ..> StageSummary : timings.csv
    ResultWriter ..> Alert : alerts.csv

    %% ---- pipeline and command line ----
    class Options {
        <<struct>>
        +optional trainPath
        +optional trainFromInputSeconds
        +string inputPath
        +string mode
        +size_t threads
        +DetectorConfig detector
        +string outputDirectory
        +size_t repeat
    }
    class Cli {
        +parse(arguments)$ Options
        +usage()$ string
    }
    class RunResult {
        <<struct>>
        +Metrics metrics
        +vector~StageTiming~ timings
        +vector~Alert~ alerts
        +vector~string~ ruleNames
        +size_t frameCount
        +size_t skippedLines
    }
    class IPipeline {
        <<interface>>
        +run(options) RunResult
    }
    class SequentialPipeline {
        +run(options) RunResult
    }
    class ConsoleReport {
        +format(options, result, timings)$ string
    }
    class BoundedBuffer~T~ {
        +push(item) bool
        +pop() optional~T~
        +close() void
    }

    Cli ..> Options : returns
    Options *-- DetectorConfig
    IPipeline <|.. SequentialPipeline
    IPipeline ..> Options
    IPipeline ..> RunResult
    RunResult *-- Metrics
    RunResult *-- StageTiming
    RunResult *-- Alert
    SequentialPipeline ..> CanLogReader
    SequentialPipeline ..> ProfileBuilder
    SequentialPipeline ..> Detector
    SequentialPipeline ..> Evaluator
    SequentialPipeline ..> StageTimer
    ConsoleReport ..> RunResult

    %% ---- colours ----
    classDef api fill:#e8e6ff,stroke:#6c63c9,color:#24206b
    classDef input fill:#dbeafe,stroke:#3b82f6,color:#10315f
    classDef learn fill:#d7f5f1,stroke:#2ec4b6,color:#0b3b36
    classDef detect fill:#fff1cc,stroke:#f2a900,color:#5a3d00
    classDef output fill:#fde2e2,stroke:#e05252,color:#6b1414
    classDef sync fill:#dcfce7,stroke:#22a06b,color:#0f4a2e
    cssClass "Options,Cli,RunResult,IPipeline,SequentialPipeline" api
    cssClass "CanFrame,LogFormat,ReadResult,CanLogReader" input
    cssClass "IdProfile,ProfileBuilder" learn
    cssClass "DetectorConfig,IdState,RuleResult,IRule,UnknownIdRule,TimingRule,RateRule,PayloadRule,Detection,Detector" detect
    cssClass "RuleStats,Metrics,Evaluator,StageTiming,StageSummary,StageTimer,Alert,ResultWriter,ConsoleReport" output
    cssClass "BoundedBuffer" sync
```

`BoundedBuffer<T>` is finished and tested but not used by the sequential pipeline; the Phase 2 pipelines
below build on it.
