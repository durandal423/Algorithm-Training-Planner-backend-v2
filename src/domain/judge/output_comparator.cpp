#include "domain/judge/output_comparator.hpp"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace judge {
namespace {

std::string trim_trailing(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' ||
                             text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }
    return text;
}

std::vector<std::string> split_whitespace(const std::string& text) {
    std::istringstream input{text};
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool parse_double(const std::string& text, double& value) {
    try {
        std::size_t consumed = 0;
        value = std::stod(text, &consumed);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

} // namespace

CompareResult compareOutputs(
    const std::string& expected,
    const std::string& actual,
    const std::string& mode,
    double float_epsilon
) {
    if (mode == "exact") {
        return expected == actual
            ? CompareResult{true, "accepted"}
            : CompareResult{false, "output differs"};
    }

    if (mode == "trim_trailing") {
        return trim_trailing(expected) == trim_trailing(actual)
            ? CompareResult{true, "accepted"}
            : CompareResult{false, "output differs after trimming trailing whitespace"};
    }

    const auto expected_tokens = split_whitespace(expected);
    const auto actual_tokens = split_whitespace(actual);
    if (expected_tokens.size() != actual_tokens.size()) {
        return {false, "token count differs"};
    }

    if (mode == "float_epsilon") {
        for (std::size_t i = 0; i < expected_tokens.size(); ++i) {
            double expected_value = 0;
            double actual_value = 0;
            if (!parse_double(expected_tokens[i], expected_value) ||
                !parse_double(actual_tokens[i], actual_value)) {
                return {false, "non numeric token in float comparison"};
            }
            if (std::fabs(expected_value - actual_value) > float_epsilon) {
                return {false, "numeric token differs"};
            }
        }
        return {true, "accepted"};
    }

    for (std::size_t i = 0; i < expected_tokens.size(); ++i) {
        if (expected_tokens[i] != actual_tokens[i]) {
            return {false, "token differs"};
        }
    }
    return {true, "accepted"};
}

} // namespace judge
