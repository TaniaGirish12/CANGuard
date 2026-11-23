#include <stdexcept>
#include <string>
#include <vector>

#include "TestRegistry.hpp"
#include "canguard/ArgumentParsing.hpp"
#include "canguard/Cli.hpp"

using canguard::Cli;
using canguard::Options;

namespace {

// Returns the error message, or an empty string when the arguments are accepted.
std::string errorFor(const std::vector<std::string>& arguments) {
    try {
        Cli::parse(arguments);
    } catch (const std::invalid_argument& error) {
        return error.what();
    }
    return "";
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE(cliAppliesTheDocumentedDefaults) {
    const Options options = Cli::parse({"--train", "normal.txt", "--input", "attack.csv"});
    CHECK(options.trainPath.has_value() && *options.trainPath == "normal.txt");
    CHECK(!options.trainFromInputSeconds.has_value());
    CHECK(options.inputPath == "attack.csv");
    CHECK(options.mode == "sequential");
    CHECK(options.threads >= 1);
    CHECK(options.detector.k == 3.0);
    CHECK(options.detector.r == 0.2);
    CHECK(options.detector.windowMs == 100.0);
    CHECK(options.detector.rateFactor == 3.0);
    CHECK(options.detector.minScore == 1);
    CHECK(options.outputDirectory == "results");
    CHECK(options.repeat == 1);
    CHECK(!options.showHelp);
}

TEST_CASE(cliParsesEveryFlag) {
    const Options options = Cli::parse({"--train-from-input", "7.5", "--input", "in.csv", "--mode", "sequential",
                                        "--threads", "6", "--k", "2.5", "--r", "0.1", "--window-ms", "50",
                                        "--rate-factor", "4", "--min-score", "2", "--out", "out_dir", "--repeat", "5"});
    CHECK(!options.trainPath.has_value());
    CHECK(options.trainFromInputSeconds.has_value() && *options.trainFromInputSeconds == 7.5);
    CHECK(options.inputPath == "in.csv");
    CHECK(options.threads == 6);
    CHECK(options.detector.k == 2.5);
    CHECK(options.detector.r == 0.1);
    CHECK(options.detector.windowMs == 50.0);
    CHECK(options.detector.rateFactor == 4.0);
    CHECK(options.detector.minScore == 2);
    CHECK(options.outputDirectory == "out_dir");
    CHECK(options.repeat == 5);
}

TEST_CASE(cliHelpSkipsAllOtherChecks) {
    CHECK(Cli::parse({"--help"}).showHelp);
    CHECK(Cli::parse({"-h"}).showHelp);
    CHECK(!Cli::usage().empty());
}

TEST_CASE(cliRequiresInputAndExactlyOneTrainingSource) {
    CHECK(contains(errorFor({"--train", "a.txt"}), "--input is required"));
    CHECK(contains(errorFor({"--input", "b.csv"}), "--train"));
    CHECK(contains(errorFor({"--input", "b.csv", "--train", "a.txt", "--train-from-input", "5"}), "not both"));
}

TEST_CASE(cliReportsUnknownOptionsAndMissingValues) {
    CHECK(contains(errorFor({"--train", "a", "--input", "b", "--bogus", "1"}), "unknown option --bogus"));
    CHECK(contains(errorFor({"--train", "a", "--input"}), "--input needs a value"));
}

TEST_CASE(cliRejectsInvalidNumbersWithHelpfulMessages) {
    const std::vector<std::string> base = {"--train", "a", "--input", "b"};
    auto withFlag = [&](const std::string& flag, const std::string& value) {
        std::vector<std::string> arguments = base;
        arguments.push_back(flag);
        arguments.push_back(value);
        return errorFor(arguments);
    };

    CHECK(contains(withFlag("--k", "abc"), "--k needs a number"));
    CHECK(contains(withFlag("--k", "-1"), "--k needs a number"));
    CHECK(contains(withFlag("--r", "1.5"), "--r needs a number between 0 and 1"));
    CHECK(contains(withFlag("--window-ms", "0"), "--window-ms needs a number greater than 0"));
    CHECK(contains(withFlag("--rate-factor", "-2"), "--rate-factor needs a number greater than 0"));
    CHECK(contains(withFlag("--min-score", "0"), "--min-score needs a whole number of at least 1"));
    CHECK(contains(withFlag("--threads", "2.5"), "--threads needs a whole number"));
    CHECK(contains(withFlag("--repeat", "0"), "--repeat needs a whole number"));
    CHECK(contains(withFlag("--repeat", "3x"), "got '3x'"));
    CHECK(contains(errorFor({"--train-from-input", "0", "--input", "b"}), "--train-from-input needs a number greater than 0"));
}

TEST_CASE(cliRejectsModesThatDoNotExistYet) {
    const std::string error = errorFor({"--train", "a", "--input", "b", "--mode", "staged"});
    CHECK(contains(error, "unknown --mode 'staged'"));
    CHECK(contains(error, "sequential"));
}

TEST_CASE(argumentParsingAcceptsOnlyCompleteNumbers) {
    CHECK(canguard::parseDouble("3.5") == 3.5);
    CHECK(canguard::parseDouble("-2") == -2.0);
    CHECK(!canguard::parseDouble("").has_value());
    CHECK(!canguard::parseDouble("1.5x").has_value());
    CHECK(!canguard::parseDouble(" 1").has_value());
    CHECK(!canguard::parseDouble("inf").has_value());
    CHECK(!canguard::parseDouble("nan").has_value());

    CHECK(canguard::parseUnsigned("42") == 42);
    CHECK(!canguard::parseUnsigned("").has_value());
    CHECK(!canguard::parseUnsigned("-1").has_value());
    CHECK(!canguard::parseUnsigned("4.2").has_value());
    CHECK(!canguard::parseUnsigned("99999999999999999999999").has_value());
}
