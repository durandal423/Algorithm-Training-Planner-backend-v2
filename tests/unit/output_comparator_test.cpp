#include "domain/judge/output_comparator.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("output comparator supports exact and whitespace modes") {
    REQUIRE(judge::compareOutputs("1\n2\n", "1\n2\n", "exact", 1e-6).accepted);
    REQUIRE_FALSE(judge::compareOutputs("1\n2\n", "1 2\n", "exact", 1e-6).accepted);

    REQUIRE(judge::compareOutputs("1  2\n", "1\n2\n", "ignore_whitespace", 1e-6).accepted);
    REQUIRE(judge::compareOutputs("answer\n\n", "answer", "trim_trailing", 1e-6).accepted);
}

TEST_CASE("output comparator supports numeric epsilon") {
    REQUIRE(judge::compareOutputs("3.14159", "3.1416", "float_epsilon", 0.001).accepted);
    REQUIRE_FALSE(judge::compareOutputs("3.14159", "3.15", "float_epsilon", 0.001).accepted);
    REQUIRE_FALSE(judge::compareOutputs("3.14159", "not-a-number", "float_epsilon", 0.001).accepted);
}
