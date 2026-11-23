#include "canguard/Evaluator.hpp"

#include <utility>

namespace canguard {

namespace {

double ratio(std::size_t numerator, std::size_t denominator) {
    return denominator == 0 ? 0.0 : static_cast<double>(numerator) / static_cast<double>(denominator);
}

}  // namespace

Evaluator::Evaluator(std::vector<std::string> ruleNames) {
    rules_.reserve(ruleNames.size());
    for (std::string& name : ruleNames) {
        RuleStats stats;
        stats.name = std::move(name);
        rules_.push_back(std::move(stats));
    }
}

void Evaluator::add(const Detection& detection, bool isAttack) {
    if (isAttack) {
        ++(detection.predictedAttack ? truePositives_ : falseNegatives_);
    } else {
        ++(detection.predictedAttack ? falsePositives_ : trueNegatives_);
    }

    for (std::size_t i = 0; i < rules_.size(); ++i) {
        if (((detection.ruleMask >> i) & 1U) == 0) continue;
        ++(isAttack ? rules_[i].truePositives : rules_[i].falsePositives);
    }
}

Metrics Evaluator::metrics() const {
    Metrics metrics;
    metrics.truePositives = truePositives_;
    metrics.falsePositives = falsePositives_;
    metrics.trueNegatives = trueNegatives_;
    metrics.falseNegatives = falseNegatives_;

    metrics.precision = ratio(truePositives_, truePositives_ + falsePositives_);
    metrics.recall = ratio(truePositives_, truePositives_ + falseNegatives_);
    metrics.falsePositiveRate = ratio(falsePositives_, falsePositives_ + trueNegatives_);
    const double precisionPlusRecall = metrics.precision + metrics.recall;
    metrics.f1 = precisionPlusRecall == 0.0 ? 0.0 : 2.0 * metrics.precision * metrics.recall / precisionPlusRecall;

    metrics.rules = rules_;
    return metrics;
}

}  // namespace canguard
