#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "nn/pipeline/advanced_pipeline.h"

namespace cosmo::nn {
namespace {

    std::string AsString(const std::vector<char>& chars) {
        return std::string(chars.begin(), chars.end());
    }

}  // namespace

TEST_CASE("OCR CTC decoder collapses blanks and repeated classes", "[ocr]") {
    const std::vector<std::string> words = {"", "A", "B"};
    const OcrCtcConfig ctc_config{0, 3};
    const std::vector<float> logits = {
        9.0F, 0.0F, 0.0F,  // blank
        0.0F, 9.0F, 0.0F,  // A
        0.0F, 9.0F, 0.0F,  // repeated A
        9.0F, 0.0F, 0.0F,  // blank separates repeated A
        0.0F, 9.0F, 0.0F,  // A
        0.0F, 0.0F, 9.0F,  // B
        0.0F, 0.0F, 9.0F   // repeated B
    };
    std::vector<std::vector<char>> output;

    auto status = DecodeOcrCtc(logits.data(), 1, 7, 3, words, ctc_config, output);
    REQUIRE(static_cast<bool>(status));
    REQUIRE(output.size() == 1);
    REQUIRE(AsString(output[0]) == "AAB");
}

TEST_CASE("OCR CTC decoder uses the correct batch offset", "[ocr]") {
    const std::vector<std::string> words = {"", "A", "B"};
    const OcrCtcConfig ctc_config{0, 3};
    const std::vector<float> logits = {
        // Batch 0: blank, A, A, blank, B, B -> AB.
        9.0F,
        0.0F,
        0.0F,
        0.0F,
        9.0F,
        0.0F,
        0.0F,
        9.0F,
        0.0F,
        9.0F,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        9.0F,
        0.0F,
        0.0F,
        9.0F,
        // Batch 1: B, B, blank, A, A, blank -> BA.
        0.0F,
        0.0F,
        9.0F,
        0.0F,
        0.0F,
        9.0F,
        9.0F,
        0.0F,
        0.0F,
        0.0F,
        9.0F,
        0.0F,
        0.0F,
        9.0F,
        0.0F,
        9.0F,
        0.0F,
        0.0F,
    };
    std::vector<std::vector<char>> output;

    auto status = DecodeOcrCtc(logits.data(), 2, 6, 3, words, ctc_config, output);
    REQUIRE(static_cast<bool>(status));
    REQUIRE(output.size() == 2);
    REQUIRE(AsString(output[0]) == "AB");
    REQUIRE(AsString(output[1]) == "BA");
}

TEST_CASE("OCR CTC decoder rejects invalid tensors and character-table mismatches", "[ocr]") {
    const std::vector<std::string> words = {"", "A", "B"};
    const OcrCtcConfig ctc_config{0, 3};
    std::vector<std::vector<char>> output = {{'x'}};

    auto invalid_status = DecodeOcrCtc(nullptr, 1, 1, 3, words, ctc_config, output);
    REQUIRE_FALSE(static_cast<bool>(invalid_status));
    REQUIRE(output.empty());

    const std::vector<float> logits = {9.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                                       0.0F, 9.0F, 0.0F, 9.0F, 0.0F, 0.0F};
    auto status                     = DecodeOcrCtc(logits.data(), 1, 3, 4, words, ctc_config, output);
    REQUIRE_FALSE(static_cast<bool>(status));
    REQUIRE(output.empty());

    const OcrCtcConfig invalid_blank{3, 3};
    status = DecodeOcrCtc(logits.data(), 1, 3, 3, words, invalid_blank, output);
    REQUIRE_FALSE(static_cast<bool>(status));
}

TEST_CASE("OCR CTC decoder supports configured Unicode and trailing tokens", "[ocr]") {
    const std::vector<std::string> words = {"blank", "京", "A", " "};
    const OcrCtcConfig ctc_config{0, 4};
    const std::vector<float> logits = {
        0.0F, 9.0F, 0.0F, 0.0F,  // 京
        0.0F, 9.0F, 0.0F, 0.0F,  // repeated 京
        9.0F, 0.0F, 0.0F, 0.0F,  // blank
        0.0F, 0.0F, 0.0F, 9.0F   // configured trailing space
    };
    std::vector<std::vector<char>> output;

    auto status = DecodeOcrCtc(logits.data(), 1, 4, 4, words, ctc_config, output);
    REQUIRE(static_cast<bool>(status));
    REQUIRE(AsString(output[0]) == "京 ");
}

}  // namespace cosmo::nn
