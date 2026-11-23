#include <cstddef>
#include <future>
#include <type_traits>
#include <vector>

#include "TestRegistry.hpp"
#include "canguard/AlertManager.hpp"
#include "canguard/IPipeline.hpp"
#include "canguard/IdPartitionedPipeline.hpp"
#include "canguard/StagedPipeline.hpp"
#include "canguard/ThreadPool.hpp"
#include "canguard/TimeWindowPipeline.hpp"

// These headers only declare interfaces, so nothing here is constructed or called: the test
// proves that they compile together and have the shape the specification describes.

using canguard::Alert;
using canguard::AlertManager;
using canguard::IdPartitionedPipeline;
using canguard::IPipeline;
using canguard::StagedPipeline;
using canguard::ThreadPool;
using canguard::TimeWindowPipeline;

TEST_CASE(phase2PipelinesImplementTheCommonInterface) {
    CHECK((std::is_base_of_v<IPipeline, IdPartitionedPipeline>));
    CHECK((std::is_base_of_v<IPipeline, TimeWindowPipeline>));
    CHECK((std::is_base_of_v<IPipeline, StagedPipeline>));

    CHECK(!std::is_abstract_v<IdPartitionedPipeline>);
    CHECK(!std::is_abstract_v<TimeWindowPipeline>);
    CHECK(!std::is_abstract_v<StagedPipeline>);
}

TEST_CASE(phase2IdPartitionedPipelineDeclaresItsWorkerChoice) {
    CHECK((std::is_same_v<decltype(IdPartitionedPipeline::workerIndexFor(0, 1)), std::size_t>));
}

TEST_CASE(phase2ThreadPoolSubmitReturnsAFutureOfTheTaskResult) {
    using IntTask = int (*)();
    using VoidTask = void (*)();
    CHECK((std::is_same_v<decltype(std::declval<ThreadPool&>().submit(std::declval<IntTask>())), std::future<int>>));
    CHECK((std::is_same_v<decltype(std::declval<ThreadPool&>().submit(std::declval<VoidTask>())), std::future<void>>));
}

TEST_CASE(phase2ThreadPoolAndAlertManagerCannotBeCopied) {
    CHECK(!std::is_copy_constructible_v<ThreadPool>);
    CHECK(!std::is_copy_assignable_v<ThreadPool>);
    CHECK(!std::is_copy_constructible_v<AlertManager>);
    CHECK(!std::is_copy_assignable_v<AlertManager>);
}

TEST_CASE(phase2AlertManagerReturnsAlertsInOrder) {
    CHECK((std::is_same_v<decltype(std::declval<AlertManager&>().finish(0)), std::vector<Alert>>));
}
