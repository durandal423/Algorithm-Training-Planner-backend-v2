#pragma once

#include "db/connection_pool.hpp"
#include "domain/problem/problem_models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace problem {

template <typename Row>
Problem problem_from_row(const Row& row) {
    Problem problem;
    problem.id = row["id"].template as<long long>();
    problem.code = row["code"].template as<std::string>();
    problem.title = row["title"].template as<std::string>();
    problem.source_platform = row["source_platform"].template as<std::string>();
    problem.source_url = row["source_url"].template get<std::string>();
    problem.difficulty = row["difficulty"].template as<int>();
    problem.estimated_minutes = row["estimated_minutes"].template as<int>();
    problem.description = row["description"].template as<std::string>("");
    problem.input_format = row["input_format"].template as<std::string>("");
    problem.output_format = row["output_format"].template as<std::string>("");
    problem.sample_input = row["sample_input"].template as<std::string>("");
    problem.sample_output = row["sample_output"].template as<std::string>("");
    problem.hint = row["hint"].template as<std::string>("");
    problem.visibility = row["visibility"].template as<std::string>();
    problem.created_by = row["created_by"].template get<long long>();
    problem.created_at = row["created_at"].template as<std::string>("");
    problem.updated_at = row["updated_at"].template as<std::string>("");
    return problem;
}

template <typename Row>
ProblemSummary problem_summary_from_row(const Row& row) {
    ProblemSummary problem;
    problem.id = row["id"].template as<long long>();
    problem.code = row["code"].template as<std::string>();
    problem.title = row["title"].template as<std::string>();
    problem.source_platform = row["source_platform"].template as<std::string>();
    problem.source_url = row["source_url"].template get<std::string>();
    problem.difficulty = row["difficulty"].template as<int>();
    problem.estimated_minutes = row["estimated_minutes"].template as<int>();
    problem.visibility = row["visibility"].template as<std::string>();
    problem.created_by = row["created_by"].template get<long long>();
    problem.created_at = row["created_at"].template as<std::string>("");
    problem.updated_at = row["updated_at"].template as<std::string>("");
    return problem;
}

template <typename Row>
Tag tag_from_row(const Row& row) {
    Tag tag;
    tag.id = row["id"].template as<long long>();
    tag.name = row["name"].template as<std::string>();
    tag.description = row["description"].template as<std::string>("");
    return tag;
}

class ProblemRepository final {
public:
    explicit ProblemRepository(ConnectionPool& pool);

    std::vector<ProblemSummary> listProblems(const ProblemFilter& filter) const;
    std::optional<Problem> getProblem(long long id, bool include_hidden) const;
    long long createProblem(const Problem& problem, const std::vector<std::string>& tag_names) const;
    bool updateProblem(long long id, const Problem& problem, const std::vector<std::string>& tag_names) const;
    bool deleteProblem(long long id) const;
    std::vector<Tag> listTags() const;

private:
    ConnectionPool& pool_;

    long long upsertTag(pqxx::transaction_base& tx, const std::string& name) const;
    void replaceProblemTags(
        pqxx::transaction_base& tx,
        long long problem_id,
        const std::vector<std::string>& names
    ) const;
    std::vector<Tag> loadTags(pqxx::transaction_base& tx, long long problem_id) const;
    void loadTagsForProblems(
        pqxx::transaction_base& tx,
        std::vector<ProblemSummary>& problems
    ) const;
};

} // namespace problem
