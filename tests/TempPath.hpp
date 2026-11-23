#pragma once

#include <unistd.h>

#include <filesystem>
#include <string>

namespace canguard::testing {

// A path in the temp folder that is unique to this process, so test runs that overlap
// (for example Debug and TSAN builds under CTest at the same time) never share files.
inline std::filesystem::path uniqueTempPath(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("canguard_" + std::to_string(::getpid()) + "_" + name);
}

}  // namespace canguard::testing
