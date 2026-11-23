#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace canguard::testing {

using TestFunction = void (*)();

struct TestCase {
    std::string name;
    TestFunction function;
};

class TestRegistry {
public:
    static TestRegistry& instance();

    void add(std::string name, TestFunction function);
    void recordFailure(const char* file, int line, const char* expression);

    // Runs every registered test and returns a process exit code (0 when all pass).
    int runAll();

private:
    std::vector<TestCase> cases_;
    std::mutex failuresMutex_;
    std::vector<std::string> currentFailures_;
};

struct TestRegistrar {
    TestRegistrar(const char* name, TestFunction function) {
        TestRegistry::instance().add(name, function);
    }
};

}  // namespace canguard::testing

#define TEST_CASE(name)                                                             \
    static void name();                                                             \
    static const ::canguard::testing::TestRegistrar registrar_##name(#name, &name); \
    static void name()

// Safe to call from any thread: failures are recorded under a mutex.
#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            ::canguard::testing::TestRegistry::instance().recordFailure(__FILE__, __LINE__, \
                                                                        #condition);       \
        }                                                                                  \
    } while (false)
