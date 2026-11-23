#include <algorithm>
#include <exception>
#include <iostream>
#include <optional>
#include <string>

#include "canguard/ArgumentParsing.hpp"
#include "canguard/SyntheticGenerator.hpp"

namespace {

constexpr int kExitUsageError = 2;
constexpr int kExitRuntimeError = 1;

void printUsage(std::ostream& out) {
    out << "usage: cangen --out PATH --seconds S [--attack dos|fuzzy|spoof|none] [--seed N]\n"
        << "  --out PATH      CSV file to write (attack-format, no header)\n"
        << "  --seconds S     length of the generated traffic, in seconds\n"
        << "  --attack NAME   attack bursts to inject, flagged T (default: none)\n"
        << "  --seed N        random seed, same seed gives the same file (default: 1)\n";
}

struct Arguments {
    std::string outPath;
    canguard::SyntheticConfig config;
};

// Prints a message and returns std::nullopt when the command line is invalid.
std::optional<Arguments> parseArguments(int argc, char** argv) {
    Arguments arguments;
    bool hasSeconds = false;

    for (int i = 1; i < argc; i += 2) {
        const std::string flag = argv[i];
        if (i + 1 >= argc) {
            std::cerr << "cangen: missing value for " << flag << '\n';
            return std::nullopt;
        }
        const std::string value = argv[i + 1];

        if (flag == "--out") {
            arguments.outPath = value;
        } else if (flag == "--seconds") {
            const auto seconds = canguard::parseDouble(value);
            if (!seconds || *seconds <= 0.0) {
                std::cerr << "cangen: --seconds needs a positive number, got '" << value << "'\n";
                return std::nullopt;
            }
            arguments.config.durationSeconds = *seconds;
            hasSeconds = true;
        } else if (flag == "--attack") {
            const auto attack = canguard::parseAttackType(value);
            if (!attack) {
                std::cerr << "cangen: --attack must be dos, fuzzy, spoof or none, got '" << value << "'\n";
                return std::nullopt;
            }
            arguments.config.attack = *attack;
        } else if (flag == "--seed") {
            const auto seed = canguard::parseUnsigned(value);
            if (!seed) {
                std::cerr << "cangen: --seed needs a non-negative integer, got '" << value << "'\n";
                return std::nullopt;
            }
            arguments.config.seed = *seed;
        } else {
            std::cerr << "cangen: unknown option " << flag << '\n';
            return std::nullopt;
        }
    }

    if (arguments.outPath.empty() || !hasSeconds) {
        std::cerr << "cangen: --out and --seconds are required\n";
        return std::nullopt;
    }
    return arguments;
}

}  // namespace

int main(int argc, char** argv) {
    const auto arguments = parseArguments(argc, argv);
    if (!arguments) {
        printUsage(std::cerr);
        return kExitUsageError;
    }

    try {
        const canguard::SyntheticGenerator generator(arguments->config);
        const auto frames = generator.generate();
        canguard::SyntheticGenerator::writeCsv(frames, arguments->outPath);

        const auto attackFrames = std::count_if(frames.begin(), frames.end(),
                                                [](const canguard::CanFrame& f) { return f.isAttack; });
        std::cout << "cangen: wrote " << frames.size() << " frames (" << attackFrames << " attack) to "
                  << arguments->outPath << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cangen: " << error.what() << '\n';
        return kExitRuntimeError;
    }
}
