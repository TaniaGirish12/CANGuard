#include "canguard/CanLogReader.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace canguard {

namespace {

constexpr std::size_t kMaxTimestampDigits = 19;  // largest digit count that always fits a uint64
constexpr std::size_t kMaxCanIdHexDigits = 8;
constexpr std::size_t kMaxDataByteHexDigits = 2;
constexpr std::uint8_t kMaxDlc = 8;

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

int hexDigitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Splits a line on one separator character without allocating.
class Tokenizer {
public:
    Tokenizer(std::string_view text, char separator, bool mergeRepeatedSeparators)
        : remaining_(text), separator_(separator), mergeRepeated_(mergeRepeatedSeparators) {}

    std::optional<std::string_view> next() {
        if (mergeRepeated_) {
            while (!remaining_.empty() && remaining_.front() == separator_) {
                remaining_.remove_prefix(1);
            }
            if (remaining_.empty()) return std::nullopt;
        } else if (finished_) {
            return std::nullopt;
        }

        const std::size_t end = remaining_.find(separator_);
        if (end == std::string_view::npos) {
            const std::string_view token = remaining_;
            remaining_ = {};
            finished_ = true;
            return token;
        }
        const std::string_view token = remaining_.substr(0, end);
        remaining_.remove_prefix(end + 1);
        return token;
    }

private:
    std::string_view remaining_;
    char separator_;
    bool mergeRepeated_;
    bool finished_ = false;
};

// Reads "digits[.digits]" as one integer divided by a power of ten. Below 2^53 both are exact,
// so the single division gives the double nearest to the decimal text.
std::optional<double> parseTimestamp(std::string_view token) {
    std::uint64_t digits = 0;
    std::size_t digitCount = 0;
    std::size_t fractionDigits = 0;
    bool seenDot = false;

    for (const char c : token) {
        if (c == '.') {
            if (seenDot || digitCount == 0) return std::nullopt;
            seenDot = true;
            continue;
        }
        if (!isDigit(c) || ++digitCount > kMaxTimestampDigits) return std::nullopt;
        digits = digits * 10 + static_cast<std::uint64_t>(c - '0');
        if (seenDot) ++fractionDigits;
    }
    if (digitCount == 0 || (seenDot && fractionDigits == 0)) return std::nullopt;

    double scale = 1.0;
    for (std::size_t i = 0; i < fractionDigits; ++i) scale *= 10.0;
    return static_cast<double>(digits) / scale;
}

std::optional<std::uint32_t> parseHex(std::string_view token, std::size_t maxDigits) {
    if (token.empty() || token.size() > maxDigits) return std::nullopt;
    std::uint32_t value = 0;
    for (const char c : token) {
        const int digit = hexDigitValue(c);
        if (digit < 0) return std::nullopt;
        value = value * 16 + static_cast<std::uint32_t>(digit);
    }
    return value;
}

std::optional<std::uint32_t> parseCanId(std::string_view token) {
    return parseHex(token, kMaxCanIdHexDigits);
}

std::optional<std::uint8_t> parseDataByte(std::string_view token) {
    const auto value = parseHex(token, kMaxDataByteHexDigits);
    if (!value) return std::nullopt;
    return static_cast<std::uint8_t>(*value);
}

std::optional<std::uint8_t> parseDlc(std::string_view token) {
    if (token.size() != 1 || !isDigit(token[0])) return std::nullopt;
    const auto dlc = static_cast<std::uint8_t>(token[0] - '0');
    if (dlc > kMaxDlc) return std::nullopt;
    return dlc;
}

std::optional<bool> parseAttackFlag(std::string_view token) {
    if (token == "T") return true;
    if (token == "R") return false;
    return std::nullopt;
}

template <typename Parser>
auto nextValue(Tokenizer& tokens, Parser parser) -> decltype(parser(std::string_view{})) {
    const auto token = tokens.next();
    if (!token) return std::nullopt;
    return parser(*token);
}

bool nextTokenIs(Tokenizer& tokens, std::string_view expected) {
    const auto token = tokens.next();
    return token && *token == expected;
}

bool readDataBytes(Tokenizer& tokens, CanFrame& frame) {
    for (std::uint8_t i = 0; i < frame.dlc; ++i) {
        const auto byte = nextValue(tokens, parseDataByte);
        if (!byte) return false;
        frame.data[i] = *byte;
    }
    return true;
}

std::optional<CanFrame> parseCsvLine(std::string_view line) {
    Tokenizer tokens(line, ',', false);
    const auto timestamp = nextValue(tokens, parseTimestamp);
    const auto id = nextValue(tokens, parseCanId);
    const auto dlc = nextValue(tokens, parseDlc);
    if (!timestamp || !id || !dlc) return std::nullopt;

    CanFrame frame;
    frame.timestamp = *timestamp;
    frame.id = *id;
    frame.dlc = *dlc;
    if (!readDataBytes(tokens, frame)) return std::nullopt;

    const auto isAttack = nextValue(tokens, parseAttackFlag);
    if (!isAttack || tokens.next()) return std::nullopt;
    frame.isAttack = *isAttack;
    return frame;
}

std::optional<CanFrame> parseNormalTextLine(std::string_view line) {
    Tokenizer tokens(line, ' ', true);
    if (!nextTokenIs(tokens, "Timestamp:")) return std::nullopt;
    const auto timestamp = nextValue(tokens, parseTimestamp);
    if (!nextTokenIs(tokens, "ID:")) return std::nullopt;
    const auto id = nextValue(tokens, parseCanId);
    if (!tokens.next()) return std::nullopt;  // constant "000" column between ID and DLC
    if (!nextTokenIs(tokens, "DLC:")) return std::nullopt;
    const auto dlc = nextValue(tokens, parseDlc);
    if (!timestamp || !id || !dlc) return std::nullopt;

    CanFrame frame;
    frame.timestamp = *timestamp;
    frame.id = *id;
    frame.dlc = *dlc;
    if (!readDataBytes(tokens, frame) || tokens.next()) return std::nullopt;
    frame.isAttack = false;
    return frame;
}

void recordLine(ReadResult& result, std::string_view line, LogFormat format,
                std::size_t lineNumber) {
    auto frame = CanLogReader::parseLine(line, format);
    if (!frame) {
        ++result.skippedLines;
        return;
    }
    frame->lineNumber = lineNumber;
    result.frames.push_back(*frame);
}

std::string readWholeFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open file: " + path);

    file.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::string content(size, '\0');
    file.read(content.data(), static_cast<std::streamsize>(size));
    if (!file) throw std::runtime_error("cannot read file: " + path);
    return content;
}

}  // namespace

CanLogReader::CanLogReader(std::string path) : path_(std::move(path)) {}

ReadResult CanLogReader::readAll() const {
    const std::string content = readWholeFile(path_);
    return parseText(content, detectFormat(content));
}

ReadResult CanLogReader::readChunk(std::size_t firstLineNumber, std::size_t lineCount) const {
    if (firstLineNumber == 0) throw std::invalid_argument("line numbers start at 1");

    std::ifstream file(path_, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open file: " + path_);

    std::string line;
    ReadResult result;
    if (!std::getline(file, line)) return result;
    const LogFormat format = detectFormat(line);
    file.clear();
    file.seekg(0, std::ios::beg);

    const std::size_t endLineNumber = firstLineNumber + lineCount;
    for (std::size_t lineNumber = 1; lineNumber < endLineNumber && std::getline(file, line);
         ++lineNumber) {
        if (lineNumber >= firstLineNumber) recordLine(result, line, format, lineNumber);
    }
    return result;
}

LogFormat CanLogReader::detectFormat(std::string_view text) {
    return text.starts_with("Timestamp:") ? LogFormat::NormalText : LogFormat::AttackCsv;
}

std::optional<CanFrame> CanLogReader::parseLine(std::string_view line, LogFormat format) {
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    return format == LogFormat::AttackCsv ? parseCsvLine(line) : parseNormalTextLine(line);
}

ReadResult CanLogReader::parseText(std::string_view text, LogFormat format,
                                   std::size_t firstLineNumber) {
    ReadResult result;
    result.frames.reserve(static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')) + 1);

    std::size_t lineNumber = firstLineNumber;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        recordLine(result, text.substr(0, end), format, lineNumber++);
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    }
    return result;
}

}  // namespace canguard
