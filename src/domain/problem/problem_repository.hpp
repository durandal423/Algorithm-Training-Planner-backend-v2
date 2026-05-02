#pragma once

#include "db/connection_pool.hpp"
#include "domain/problem/problem_models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace problem {

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
