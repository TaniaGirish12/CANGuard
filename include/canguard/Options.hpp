#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <thread>

#include "canguard/DetectorConfig.hpp"

namespace canguard {

struct Options {
    std::optional<std::string> trainPath;            // --train: attack-free file
    std::optional<double> trainFromInputSeconds;     // --train-from-input: learn from the input's first seconds
    std::string inputPath;                           // --input
    std::string mode = "sequential";                 // --mode
    std::size_t threads = std::max(1U, std::thread::hardware_concurrency());  // --threads (Phase 2)
    DetectorConfig detector;                         // --k --r --window-ms --rate-factor --min-score
    std::string outputDirectory = "results";         // --out
    std::size_t repeat = 1;                          // --repeat
    bool showHelp = false;                           // --help
};

}  // namespace canguard
