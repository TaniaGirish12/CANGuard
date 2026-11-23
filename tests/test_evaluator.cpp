#include <cmath>

#include "TestRegistry.hpp"
#include "canguard/Evaluator.hpp"

using canguard::Detection;
using canguard::Evaluator;
using canguard::Metrics;

namespace {

constexpr canguard::RuleMask kRuleA = 1U << 0;
constexpr canguard::RuleMask kRuleB = 1U << 1;

bool nearlyEqual(double a, double b) {
    return std::abs(a - b) < 1e-9;
}

Detection makeDetection(bool predictedAttack, canguard::RuleMask ruleMask) {
    Detection detection;
    detection.predictedAttack = predictedAttack;
    detection.ruleMask = ruleMask;
    return detection;
}

Evaluator makeEvaluator() {
    return Evaluator({"A", "B"});
}

}  // namespace

TEST_CASE(evaluatorComputesConfusionMatrixAndMetricsOnSmallExample) {
    Evaluator evaluator = makeEvaluator();
    evaluator.add(makeDetection(true, kRuleA | kRuleB), true);   // TP
    evaluator.add(makeDetection(true, kRuleA), true);            // TP
    evaluator.add(makeDetection(true, kRuleB), true);            // TP
    evaluator.add(makeDetection(true, kRuleA | kRuleB), true);   // TP
    evaluator.add(makeDetection(false, 0), true);                // FN
    evaluator.add(makeDetection(true, kRuleA), false);           // FP
    for (int i = 0; i < 4; ++i) evaluator.add(makeDetection(false, 0), false);  // TN
    evaluator.add(makeDetection(false, kRuleB), false);          // TN, although rule B fired

    const Metrics metrics = evaluator.metrics();
    CHECK(metrics.truePositives == 4);
    CHECK(metrics.falsePositives == 1);
    CHECK(metrics.trueNegatives == 5);
    CHECK(metrics.falseNegatives == 1);
    CHECK(nearlyEqual(metrics.precision, 0.8));
    CHECK(nearlyEqual(metrics.recall, 0.8));
    CHECK(nearlyEqual(metrics.f1, 0.8));
    CHECK(nearlyEqual(metrics.falsePositiveRate, 1.0 / 6.0));
}

TEST_CASE(evaluatorCountsPerRuleFiresIndependentlyOfTheFinalDecision) {
    Evaluator evaluator = makeEvaluator();
    evaluator.add(makeDetection(true, kRuleA | kRuleB), true);
    evaluator.add(makeDetection(true, kRuleA), true);
    evaluator.add(makeDetection(true, kRuleB), true);
    evaluator.add(makeDetection(true, kRuleA | kRuleB), true);
    evaluator.add(makeDetection(false, 0), true);
    evaluator.add(makeDetection(true, kRuleA), false);
    evaluator.add(makeDetection(false, kRuleB), false);  // rule B fired but minScore kept it a negative

    const Metrics metrics = evaluator.metrics();
    CHECK(metrics.rules.size() == 2);
    if (metrics.rules.size() != 2) return;

    CHECK(metrics.rules[0].name == "A");
    CHECK(metrics.rules[0].truePositives == 3);
    CHECK(metrics.rules[0].falsePositives == 1);
    CHECK(metrics.rules[0].fired() == 4);

    CHECK(metrics.rules[1].name == "B");
    CHECK(metrics.rules[1].truePositives == 3);
    CHECK(metrics.rules[1].falsePositives == 1);
    CHECK(metrics.rules[1].fired() == 4);
}

TEST_CASE(evaluatorF1CombinesPrecisionAndRecall) {
    Evaluator evaluator = makeEvaluator();
    evaluator.add(makeDetection(true, kRuleA), true);   // TP
    evaluator.add(makeDetection(true, kRuleA), true);   // TP
    evaluator.add(makeDetection(true, kRuleA), false);  // FP
    evaluator.add(makeDetection(true, kRuleA), false);  // FP

    const Metrics metrics = evaluator.metrics();
    CHECK(nearlyEqual(metrics.precision, 0.5));
    CHECK(nearlyEqual(metrics.recall, 1.0));
    CHECK(nearlyEqual(metrics.f1, 2.0 / 3.0));
    CHECK(nearlyEqual(metrics.falsePositiveRate, 1.0));
}

TEST_CASE(evaluatorReportsZeroRatiosWhenNothingWasEvaluated) {
    const Metrics metrics = makeEvaluator().metrics();
    CHECK(metrics.truePositives == 0);
    CHECK(metrics.precision == 0.0);
    CHECK(metrics.recall == 0.0);
    CHECK(metrics.f1 == 0.0);
    CHECK(metrics.falsePositiveRate == 0.0);
    CHECK(metrics.rules.size() == 2);
}

TEST_CASE(evaluatorHandlesTrafficWithoutAnyAttackOrDetection) {
    Evaluator evaluator = makeEvaluator();
    for (int i = 0; i < 10; ++i) evaluator.add(makeDetection(false, 0), false);

    const Metrics metrics = evaluator.metrics();
    CHECK(metrics.trueNegatives == 10);
    CHECK(metrics.precision == 0.0);
    CHECK(metrics.recall == 0.0);
    CHECK(metrics.falsePositiveRate == 0.0);
}
