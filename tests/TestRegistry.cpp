#include "TestRegistry.hpp"

#include <exception>
#include <iostream>
#include <utility>

namespace canguard::testing {

TestRegistry& TestRegistry::instance() {
    static TestRegistry registry;
    return registry;
}

void TestRegistry::add(std::string name, TestFunction function) {
    cases_.push_back({std::move(name), function});
}

void TestRegistry::recordFailure(const char* file, int line, const char* expression) {
    std::lock_guard<std::mutex> lock(failuresMutex_);
    currentFailures_.push_back(std::string(file) + ":" + std::to_string(line) + ": CHECK(" +
                               expression + ") failed");
}

int TestRegistry::runAll() {
    int failedTests = 0;
    for (const TestCase& testCase : cases_) {
        currentFailures_.clear();
        try {
            testCase.function();
        } catch (const std::exception& error) {
            recordFailure(__FILE__, __LINE__, error.what());
        } catch (...) {
            recordFailure(__FILE__, __LINE__, "unknown exception");
        }

        if (currentFailures_.empty()) {
            std::cout << "[ PASS ] " << testCase.name << '\n';
            continue;
        }
        ++failedTests;
        std::cout << "[ FAIL ] " << testCase.name << '\n';
        for (const std::string& failure : currentFailures_) {
            std::cout << "         " << failure << '\n';
        }
    }

    std::cout << cases_.size() - failedTests << "/" << cases_.size() << " tests passed\n";
    return failedTests == 0 ? 0 : 1;
}

}  // namespace canguard::testing
