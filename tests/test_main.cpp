#include "TestRegistry.hpp"

int main() {
    return canguard::testing::TestRegistry::instance().runAll();
}
