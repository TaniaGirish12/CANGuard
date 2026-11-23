#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "canguard/Detector.hpp"

namespace canguard {

// How often one rule fired, judged against the ground-truth label of each message it fired on.
struct RuleStats {
    std::string name;
    std::size_t truePositives = 0;   // fired on an attack message
    std::size_t falsePositives = 0;  // fired on a normal message

    std::size_t fired() const { return truePositives + falsePositives; }
};

struct Metrics {
    std::size_t truePositives = 0;
    std::size_t falsePositives = 0;
    std::size_t trueNegatives = 0;
    std::size_t falseNegatives = 0;

    double precision = 0.0;
    double recall = 0.0;
    double f1 = 0.0;
    double falsePositiveRate = 0.0;

    std::vector<RuleStats> rules;
};

// Attack is the positive class. A ratio whose denominator is zero is reported as 0.
class Evaluator {
public:
    // ruleNames[i] names the rule behind bit i of Detection::ruleMask.
    explicit Evaluator(std::vector<std::string> ruleNames);

    void add(const Detection& detection, bool isAttack);

    Metrics metrics() const;

private:
    std::size_t truePositives_ = 0;
    std::size_t falsePositives_ = 0;
    std::size_t trueNegatives_ = 0;
    std::size_t falseNegatives_ = 0;
    std::vector<RuleStats> rules_;
};

}  // namespace canguard
