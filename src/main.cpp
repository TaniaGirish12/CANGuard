#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "canguard/Cli.hpp"
#include "canguard/ConsoleReport.hpp"
#include "canguard/ResultWriter.hpp"
#include "canguard/SequentialPipeline.hpp"

namespace {

constexpr int kExitRuntimeError = 1;
constexpr int kExitUsageError = 2;

std::unique_ptr<canguard::IPipeline> makePipeline(const std::string& mode) {
    if (mode == "sequential") return std::make_unique<canguard::SequentialPipeline>();
    throw std::invalid_argument("mode not available: " + mode);
}

}  // namespace

int main(int argc, char** argv) {
    canguard::Options options;
    try {
        options = canguard::Cli::parse(std::vector<std::string>(argv + 1, argv + argc));
    } catch (const std::invalid_argument& error) {
        std::cerr << "canguard: " << error.what() << "\n\n" << canguard::Cli::usage();
        return kExitUsageError;
    }
    if (options.showHelp) {
        std::cout << canguard::Cli::usage();
        return 0;
    }

    try {
        const std::unique_ptr<canguard::IPipeline> pipeline = makePipeline(options.mode);

        canguard::RunResult lastRun;
        std::vector<std::vector<canguard::StageTiming>> timingsPerRun;
        for (std::size_t run = 0; run < options.repeat; ++run) {
            lastRun = pipeline->run(options);
            timingsPerRun.push_back(lastRun.timings);
        }
        const auto timings = canguard::StageTimer::summarize(timingsPerRun);

        const canguard::ResultWriter writer(options.outputDirectory, lastRun.ruleNames);
        writer.writeMetrics(lastRun.metrics);
        writer.writeTimings(timings);
        writer.writeAlerts(lastRun.alerts);

        std::cout << canguard::ConsoleReport::format(options, lastRun, timings);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "canguard: " << error.what() << '\n';
        return kExitRuntimeError;
    }
}
