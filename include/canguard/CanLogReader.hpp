#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "canguard/CanFrame.hpp"

namespace canguard {

enum class LogFormat {
    AttackCsv,   // Timestamp,CAN_ID,DLC,DATA[0..DLC-1],Flag (no header)
    NormalText,  // Timestamp: t  ID: id  000  DLC: n  b0 b1 ...
};

struct ReadResult {
    std::vector<CanFrame> frames;
    std::size_t skippedLines = 0;
};

class CanLogReader {
public:
    explicit CanLogReader(std::string path);

    // Parses the whole file. Throws std::runtime_error if it cannot be read.
    ReadResult readAll() const;

    // Parses lineCount lines starting at firstLineNumber (1-based) without loading the whole file.
    ReadResult readChunk(std::size_t firstLineNumber, std::size_t lineCount) const;

    // Decides the format from the beginning of a file or chunk.
    static LogFormat detectFormat(std::string_view text);

    // Returns std::nullopt for a malformed line. lineNumber of the frame is left at 0.
    static std::optional<CanFrame> parseLine(std::string_view line, LogFormat format);

    // Parses every line of an in-memory buffer, counting malformed lines instead of failing.
    static ReadResult parseText(std::string_view text, LogFormat format,
                                std::size_t firstLineNumber = 1);

private:
    std::string path_;
};

}  // namespace canguard
