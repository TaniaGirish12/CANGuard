#pragma once

#include <string>
#include <vector>

#include "canguard/Options.hpp"

namespace canguard {

class Cli {
public:
    // arguments excludes the program name. Throws std::invalid_argument with a message that says
    // what is wrong and what is expected. --help skips all other checks.
    static Options parse(const std::vector<std::string>& arguments);

    static std::string usage();
};

}  // namespace canguard
