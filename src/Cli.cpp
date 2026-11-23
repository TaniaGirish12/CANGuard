#include "canguard/Cli.hpp"

#include <functional>
#include <stdexcept>
#include <unordered_map>

#include "canguard/ArgumentParsing.hpp"

namespace canguard {

namespace {

using Handler = std::function<void(Options&, const std::string& value)>;

[[noreturn]] void failBadValue(const std::string& flag, const std::string& expected, const std::string& value) {
    throw std::invalid_argument(flag + " needs " + expected + ", got '" + value + "'");
}

double numberAtLeast(const std::string& flag, const std::string& text, double minimum) {
    const auto value = parseDouble(text);
    if (!value || *value < minimum) {
        failBadValue(flag, "a number of at least " + std::to_string(static_cast<int>(minimum)), text);
    }
    return *value;
}

double numberAboveZero(const std::string& flag, const std::string& text) {
    const auto value = parseDouble(text);
    if (!value || *value <= 0.0) failBadValue(flag, "a number greater than 0", text);
    return *value;
}

double numberBetweenZeroAndOne(const std::string& flag, const std::string& text) {
    const auto value = parseDouble(text);
    if (!value || *value < 0.0 || *value > 1.0) failBadValue(flag, "a number between 0 and 1", text);
    return *value;
}

std::size_t positiveInteger(const std::string& flag, const std::string& text) {
    const auto value = parseUnsigned(text);
    if (!value || *value < 1) failBadValue(flag, "a whole number of at least 1", text);
    return static_cast<std::size_t>(*value);
}

std::unordered_map<std::string, Handler> makeHandlers() {
    return {
        {"--train", [](Options& o, const std::string& v) { o.trainPath = v; }},
        {"--train-from-input",
         [](Options& o, const std::string& v) { o.trainFromInputSeconds = numberAboveZero("--train-from-input", v); }},
        {"--input", [](Options& o, const std::string& v) { o.inputPath = v; }},
        {"--mode",
         [](Options& o, const std::string& v) {
             if (v != "sequential") {
                 throw std::invalid_argument("unknown --mode '" + v +
                                             "' (available: sequential; idpart, timewin and staged come in Phase 2)");
             }
             o.mode = v;
         }},
        {"--threads", [](Options& o, const std::string& v) { o.threads = positiveInteger("--threads", v); }},
        {"--k", [](Options& o, const std::string& v) { o.detector.k = numberAtLeast("--k", v, 0.0); }},
        {"--r", [](Options& o, const std::string& v) { o.detector.r = numberBetweenZeroAndOne("--r", v); }},
        {"--window-ms", [](Options& o, const std::string& v) { o.detector.windowMs = numberAboveZero("--window-ms", v); }},
        {"--rate-factor",
         [](Options& o, const std::string& v) { o.detector.rateFactor = numberAboveZero("--rate-factor", v); }},
        {"--min-score", [](Options& o, const std::string& v) { o.detector.minScore = positiveInteger("--min-score", v); }},
        {"--out", [](Options& o, const std::string& v) { o.outputDirectory = v; }},
        {"--repeat", [](Options& o, const std::string& v) { o.repeat = positiveInteger("--repeat", v); }},
    };
}

}  // namespace

Options Cli::parse(const std::vector<std::string>& arguments) {
    static const std::unordered_map<std::string, Handler> handlers = makeHandlers();

    Options options;
    for (std::size_t i = 0; i < arguments.size(); i += 2) {
        const std::string& flag = arguments[i];
        if (flag == "--help" || flag == "-h") {
            options.showHelp = true;
            return options;
        }

        const auto handler = handlers.find(flag);
        if (handler == handlers.end()) throw std::invalid_argument("unknown option " + flag);
        if (i + 1 >= arguments.size()) throw std::invalid_argument(flag + " needs a value");
        handler->second(options, arguments[i + 1]);
    }

    if (options.inputPath.empty()) throw std::invalid_argument("--input is required");
    if (options.trainPath && options.trainFromInputSeconds) {
        throw std::invalid_argument("use either --train or --train-from-input, not both");
    }
    if (!options.trainPath && !options.trainFromInputSeconds) {
        throw std::invalid_argument("one of --train PATH or --train-from-input SEC is required");
    }
    return options;
}

std::string Cli::usage() {
    return "usage: canguard (--train PATH | --train-from-input SEC) --input PATH [options]\n"
           "\n"
           "  --train PATH          normal (attack-free) traffic file used for training\n"
           "  --train-from-input SEC  train on the R-flagged messages of the first SEC seconds of the input\n"
           "  --input PATH          file to analyse (required)\n"
           "  --mode NAME           sequential (default); Phase 2 adds idpart, timewin, staged\n"
           "  --threads N           worker threads, used by Phase 2 modes (default: hardware concurrency)\n"
           "  --k X                 timing rule: standard deviations below the mean (default 3)\n"
           "  --r X                 timing rule: fraction of the mean as a lower bound, 0 to 1 (default 0.2)\n"
           "  --window-ms X         rate rule: window length in milliseconds (default 100)\n"
           "  --rate-factor X       rate rule: allowed multiple of the expected count (default 3)\n"
           "  --min-score N         rules that must fire to call a message an attack (default 1)\n"
           "  --out DIR             output folder for metrics.csv, timings.csv, alerts.csv (default results)\n"
           "  --repeat N            run N times and report mean and std of stage timings (default 1)\n"
           "  --help                show this message\n";
}

}  // namespace canguard
