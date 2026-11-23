#include "canguard/SequentialPipeline.hpp"

#include <stdexcept>

#include "canguard/CanLogReader.hpp"
#include "canguard/Detector.hpp"
#include "canguard/ProfileBuilder.hpp"

namespace canguard {

namespace {

// Only normal messages teach the profiles, whichever source they come from.
ProfileMap learnProfiles(const Options& options, const std::vector<CanFrame>& input,
                         const std::vector<CanFrame>& trainingFile) {
    ProfileBuilder builder;
    std::size_t learnedFrames = 0;

    if (options.trainPath) {
        for (const CanFrame& frame : trainingFile) {
            if (frame.isAttack) continue;
            builder.add(frame);
            ++learnedFrames;
        }
        if (learnedFrames == 0) throw std::runtime_error("no normal messages found in " + *options.trainPath);
    } else {
        const double trainingSeconds = *options.trainFromInputSeconds;
        const double trainingEnd = input.empty() ? 0.0 : input.front().timestamp + trainingSeconds;
        for (const CanFrame& frame : input) {
            if (frame.timestamp >= trainingEnd) break;
            if (frame.isAttack) continue;
            builder.add(frame);
            ++learnedFrames;
        }
        if (learnedFrames == 0) {
            throw std::runtime_error("no normal (R-flagged) messages in the first " + std::to_string(trainingSeconds) +
                                     " seconds of " + options.inputPath);
        }
    }
    return builder.build();
}

}  // namespace

RunResult SequentialPipeline::run(const Options& options) {
    StageTimer timer;

    ReadResult input;
    ReadResult trainingFile;
    timer.measure("read+parse", [&] {
        input = CanLogReader(options.inputPath).readAll();
        if (options.trainPath) trainingFile = CanLogReader(*options.trainPath).readAll();
    });

    ProfileMap profiles;
    timer.measure("train", [&] { profiles = learnProfiles(options, input.frames, trainingFile.frames); });

    Detector detector(profiles, options.detector);
    std::vector<Detection> detections;
    detections.reserve(input.frames.size());
    timer.measure("detect", [&] {
        for (const CanFrame& frame : input.frames) detections.push_back(detector.process(frame));
    });

    RunResult result;
    for (std::size_t i = 0; i < detector.ruleCount(); ++i) result.ruleNames.emplace_back(detector.ruleName(i));

    Evaluator evaluator(result.ruleNames);
    timer.measure("evaluate", [&] {
        for (std::size_t i = 0; i < detections.size(); ++i) {
            const CanFrame& frame = input.frames[i];
            evaluator.add(detections[i], frame.isAttack);
            if (detections[i].predictedAttack) {
                result.alerts.push_back({frame.timestamp, frame.id, detections[i].ruleMask, frame.isAttack});
            }
        }
    });

    result.metrics = evaluator.metrics();
    result.timings = timer.timings();
    result.frameCount = input.frames.size();
    result.skippedLines = input.skippedLines;
    result.trainingSkippedLines = trainingFile.skippedLines;
    return result;
}

}  // namespace canguard
