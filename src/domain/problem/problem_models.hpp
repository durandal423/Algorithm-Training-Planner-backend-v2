#pragma once

#include <optional>
#include <string>
#include <vector>

namespace problem {

struct Tag {
    long long id{};
    std::string name;
    std::string description;
};

struct Problem {
    long long id{};
    std::string code;
    std::string title;
    std::string source_platform{"local"};
    std::optional<std::string> source_url;
    int difficulty{};
    int estimated_minutes{30};
    std::string description;
    std::string input_format;
    std::string output_format;
    std::string sample_input;
    std::string sample_output;
    std::string hint;
    std::string visibility{"public"};
    std::optional<long long> created_by;
    std::string created_at;
    std::string updated_at;
    std::vector<Tag> tags;
};

struct ProblemFilter {
    std::string keyword;
    std::string tag;
    std::optional<int> difficulty_min;
    std::optional<int> difficulty_max;
    bool include_hidden{};
    int limit{50};
    int offset{};
};

} // namespace problem
