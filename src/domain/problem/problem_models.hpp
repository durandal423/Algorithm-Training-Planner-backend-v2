#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace problem {

struct Tag {
    long long id{};
    std::string name;
    std::string description;
};

inline void to_json(nlohmann::json& json, const Tag& tag) {
    json = {
        {"id", tag.id},
        {"name", tag.name},
        {"description", tag.description}
    };
}

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

inline void to_json(nlohmann::json& json, const Problem& problem) {
    json = {
        {"id", problem.id},
        {"code", problem.code},
        {"title", problem.title},
        {"sourcePlatform", problem.source_platform},
        {"sourceUrl", problem.source_url},
        {"difficulty", problem.difficulty},
        {"estimatedMinutes", problem.estimated_minutes},
        {"description", problem.description},
        {"inputFormat", problem.input_format},
        {"outputFormat", problem.output_format},
        {"sampleInput", problem.sample_input},
        {"sampleOutput", problem.sample_output},
        {"hint", problem.hint},
        {"visibility", problem.visibility},
        {"createdBy", problem.created_by},
        {"createdAt", problem.created_at},
        {"updatedAt", problem.updated_at},
        {"tags", problem.tags}
    };
}

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
