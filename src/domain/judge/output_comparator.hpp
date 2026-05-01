#pragma once

#include <string>

namespace judge {

struct CompareResult {
    bool accepted{};
    std::string message;
};

CompareResult compareOutputs(
    const std::string& expected,
    const std::string& actual,
    const std::string& mode,
    double float_epsilon
);

} // namespace judge
