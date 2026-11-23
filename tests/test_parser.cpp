#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "TestRegistry.hpp"
#include "TempPath.hpp"
#include "canguard/CanLogReader.hpp"

using canguard::CanFrame;
using canguard::CanLogReader;
using canguard::LogFormat;
using canguard::ReadResult;

namespace {

bool nearlyEqual(double a, double b) {
    return std::abs(a - b) < 1e-9;
}

std::filesystem::path writeTempFile(const std::string& name, const std::string& content) {
    const auto path = canguard::testing::uniqueTempPath(name);
    std::ofstream(path, std::ios::binary) << content;
    return path;
}

}  // namespace

TEST_CASE(parsesCsvLineWithDlc8) {
    const auto frame = CanLogReader::parseLine("1478198376.389427,0316,8,05,21,68,09,21,21,00,6f,R",
                                               LogFormat::AttackCsv);
    CHECK(frame.has_value());
    if (!frame) return;
    CHECK(nearlyEqual(frame->timestamp, 1478198376.389427));
    CHECK(frame->id == 0x316);
    CHECK(frame->dlc == 8);
    const std::array<std::uint8_t, 8> expected{0x05, 0x21, 0x68, 0x09, 0x21, 0x21, 0x00, 0x6f};
    CHECK(frame->data == expected);
    CHECK(!frame->isAttack);
}

TEST_CASE(parsesCsvLineWithDlc2AndFlagAsLastField) {
    const auto frame =
        CanLogReader::parseLine("1478198376.500000,0545,2,d8,01,T", LogFormat::AttackCsv);
    CHECK(frame.has_value());
    if (!frame) return;
    CHECK(frame->id == 0x545);
    CHECK(frame->dlc == 2);
    CHECK(frame->data[0] == 0xd8);
    CHECK(frame->data[1] == 0x01);
    CHECK(frame->data[2] == 0x00);
    CHECK(frame->isAttack);
}

TEST_CASE(parsesCsvLineWithDlc0) {
    const auto frame = CanLogReader::parseLine("1478198376.500000,0000,0,T", LogFormat::AttackCsv);
    CHECK(frame.has_value());
    if (!frame) return;
    CHECK(frame->dlc == 0);
    CHECK(frame->isAttack);
}

TEST_CASE(parsesCsvLineWithCrlfEnding) {
    const auto frame =
        CanLogReader::parseLine("1478198376.389636,0000,2,00,00,T\r", LogFormat::AttackCsv);
    CHECK(frame.has_value());
    if (!frame) return;
    CHECK(frame->isAttack);
}

TEST_CASE(rejectsMalformedCsvLines) {
    const char* malformedLines[] = {
        "",
        "garbage",
        "1478198376.389427,0316,8,05,21,R",           // fewer data bytes than DLC
        "1478198376.389427,0316,2,05,21,R,extra",     // extra field after the flag
        "1478198376.389427,zzzz,2,05,21,R",           // CAN ID is not hex
        "1478198376.389427,0316,9,05,21,R",           // DLC above 8
        "1478198376.389427,0316,2,05,xx,R",           // data byte is not hex
        "1478198376.389427,0316,2,05,21,X",           // unknown flag
        "1478198376.389427,0316,2,05,21",             // flag missing
        "abc,0316,2,05,21,R",                         // timestamp is not a number
        "Timestamp,CAN_ID,DLC,DATA[0],DATA[1],Flag",  // header line
    };
    for (const char* line : malformedLines) {
        CHECK(!CanLogReader::parseLine(line, LogFormat::AttackCsv).has_value());
    }
}

TEST_CASE(parseTextSkipsAndCountsMalformedLines) {
    const std::string text =
        "1478198376.389427,0316,8,05,21,68,09,21,21,00,6f,R\r\n"
        "garbage\r\n"
        "1478198376.389427,0316,8,05,21,R\r\n"
        "\r\n"
        "1478198376.389636,0000,2,00,00,T";  // last line has no newline

    const ReadResult result = CanLogReader::parseText(text, LogFormat::AttackCsv);
    CHECK(result.frames.size() == 2);
    CHECK(result.skippedLines == 3);
    if (result.frames.size() != 2) return;
    CHECK(result.frames[0].lineNumber == 1);
    CHECK(result.frames[1].lineNumber == 5);
    CHECK(result.frames[1].isAttack);
}

TEST_CASE(parsesAttackFreeTextLine) {
    const auto frame = CanLogReader::parseLine(
        "Timestamp: 1479121434.850202        ID: 0350    000    DLC: 8    05 28 84 66 6d 00 00 a2",
        LogFormat::NormalText);
    CHECK(frame.has_value());
    if (!frame) return;
    CHECK(nearlyEqual(frame->timestamp, 1479121434.850202));
    CHECK(frame->id == 0x350);
    CHECK(frame->dlc == 8);
    const std::array<std::uint8_t, 8> expected{0x05, 0x28, 0x84, 0x66, 0x6d, 0x00, 0x00, 0xa2};
    CHECK(frame->data == expected);
    CHECK(!frame->isAttack);
}

TEST_CASE(parsesAttackFreeTextLineWithShortDlc) {
    const auto frame = CanLogReader::parseLine(
        "Timestamp: 1479121434.854290        ID: 02b0    000    DLC: 5    8d ff 00 07 02",
        LogFormat::NormalText);
    CHECK(frame.has_value());
    if (!frame) return;
    CHECK(frame->dlc == 5);
    CHECK(frame->data[4] == 0x02);
    CHECK(frame->data[5] == 0x00);
}

TEST_CASE(rejectsMalformedAttackFreeTextLines) {
    const char* malformedLines[] = {
        "Timestamp: 1479121941.287013",                                         // truncated line
        "Timestamp: 1479121434.850202   ID: 0350   000   DLC: 8   05 28",       // too few bytes
        "Timestamp: 1479121434.850202   ID: 0350   000   DLC: 2   05 28 99",    // too many bytes
        "Timestamp: 1479121434.850202   ID: 0350   000   DLC: 2   05 zz",       // data byte not hex
        "1479121434.850202,0350,2,05,28,R",                                     // CSV line in text file
    };
    for (const char* line : malformedLines) {
        CHECK(!CanLogReader::parseLine(line, LogFormat::NormalText).has_value());
    }
}

TEST_CASE(detectsFormatFromFileStart) {
    CHECK(CanLogReader::detectFormat("Timestamp: 1479121434.850202   ID: 0350") ==
          LogFormat::NormalText);
    CHECK(CanLogReader::detectFormat("1478198376.389427,0316,8,05,21,68,09,21,21,00,6f,R") ==
          LogFormat::AttackCsv);
    CHECK(CanLogReader::detectFormat("") == LogFormat::AttackCsv);
}

TEST_CASE(readAllDetectsFormatAndCountsSkippedLines) {
    const auto path = writeTempFile(
        "canguard_test_attack_free.txt",
        "Timestamp: 1479121434.850202        ID: 0350    000    DLC: 8    05 28 84 66 6d 00 00 a2\n"
        "Timestamp: 1479121434.854290        ID: 02b0    000    DLC: 5    8d ff 00 07 02\n"
        "Timestamp: 1479121941.287013");
    const ReadResult result = CanLogReader(path.string()).readAll();
    std::filesystem::remove(path);

    CHECK(result.frames.size() == 2);
    CHECK(result.skippedLines == 1);
}

TEST_CASE(readChunkReturnsRequestedLinesWithOriginalLineNumbers) {
    const auto path = writeTempFile("canguard_test_chunk.csv",
                                    "1478198376.000001,0001,1,01,R\r\n"
                                    "1478198376.000002,0002,1,02,R\r\n"
                                    "1478198376.000003,0003,1,03,T\r\n"
                                    "bad line\r\n"
                                    "1478198376.000005,0005,1,05,R\r\n");
    const CanLogReader reader(path.string());
    const ReadResult chunk = reader.readChunk(2, 3);
    const ReadResult beyondEnd = reader.readChunk(5, 100);
    std::filesystem::remove(path);

    CHECK(chunk.frames.size() == 2);
    CHECK(chunk.skippedLines == 1);
    if (chunk.frames.size() == 2) {
        CHECK(chunk.frames[0].id == 2);
        CHECK(chunk.frames[0].lineNumber == 2);
        CHECK(chunk.frames[1].id == 3);
        CHECK(chunk.frames[1].isAttack);
    }
    CHECK(beyondEnd.frames.size() == 1);
    if (beyondEnd.frames.size() == 1) CHECK(beyondEnd.frames[0].lineNumber == 5);
}

TEST_CASE(readAllThrowsWhenFileIsMissing) {
    bool threw = false;
    try {
        CanLogReader("/nonexistent/canguard_missing.csv").readAll();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}
