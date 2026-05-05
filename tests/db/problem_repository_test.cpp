#include "domain/problem/problem_repository.hpp"
#include "test_support/db_test_utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <pqxx/pqxx>

#include <string>
#include <vector>

namespace {

problem::Problem make_problem(const std::string& code, const std::string& title) {
    problem::Problem problem;
    problem.code = code;
    problem.title = title;
    problem.difficulty = 1200;
    problem.estimated_minutes = 20;
    problem.visibility = "public";
    return problem;
}

problem::Problem make_hidden_problem(const std::string& code, const std::string& title) {
    auto problem = make_problem(code, title);
    problem.visibility = "hidden";
    return problem;
}

std::vector<std::string> codes_from(const std::vector<problem::ProblemSummary>& problems) {
    std::vector<std::string> codes;
    codes.reserve(problems.size());
    for (const auto& problem : problems) {
        codes.push_back(problem.code);
    }
    return codes;
}

} // namespace

TEST_CASE("problem search filters by tag name, id, and comma-separated tag values") {
    if (!test_support::dbTestsEnabled()) {
        SKIP("set RUN_DB_TESTS=1 to run database integration tests");
    }

    test_support::ensureDatabaseReady();
    const auto prefix = test_support::uniqueName("DB_PROBLEM_TAG_");
    const auto graph_tag = prefix + "_GraphSearch";
    const auto dp_tag = prefix + "_DP";
    const auto greedy_tag = prefix + "_Greedy";
    const auto graph_code = prefix + "_GRAPH";
    const auto greedy_code = prefix + "_GREEDY";
    const auto hidden_code = prefix + "_HIDDEN";

    struct Cleanup {
        std::vector<std::string> codes;
        std::vector<std::string> tags;
        ~Cleanup() {
            try {
                pqxx::connection conn{Database::appConninfo()};
                pqxx::work tx{conn};
                for (const auto& code : codes) {
                    tx.exec(
                        "DELETE FROM problems WHERE code = $1",
                        pqxx::params{code}
                    ).no_rows();
                }
                for (const auto& tag : tags) {
                    tx.exec(
                        "DELETE FROM tags WHERE name = $1",
                        pqxx::params{tag}
                    ).no_rows();
                }
                tx.commit();
            } catch (...) {
            }
        }
    } cleanup{{graph_code, greedy_code, hidden_code}, {graph_tag, dp_tag, greedy_tag}};

    ConnectionPool pool{Database::appConninfo(), 4};
    problem::ProblemRepository repository{pool};
    const auto public_count_before = repository.countProblems(false);
    const auto all_count_before = repository.countProblems(true);

    const auto graph_id = repository.createProblem(
        make_problem(graph_code, "Graph Search Exercise"),
        {graph_tag, dp_tag}
    );
    repository.createProblem(
        make_problem(greedy_code, "Greedy Exercise"),
        {greedy_tag}
    );
    repository.createProblem(
        make_hidden_problem(hidden_code, "Hidden Exercise"),
        {graph_tag}
    );

    REQUIRE(repository.countProblems(false) == public_count_before + 2);
    REQUIRE(repository.countProblems(true) == all_count_before + 3);

    const auto graph_problem = repository.getProblem(graph_id, true);
    REQUIRE(graph_problem.has_value());
    REQUIRE(graph_problem->tags.size() == 2);
    const auto tag_id = graph_problem->tags.front().name == graph_tag
        ? graph_problem->tags.front().id
        : graph_problem->tags.back().id;

    problem::ProblemFilter filter;
    filter.keyword = prefix;
    filter.tag = "  " + graph_tag + "  ";
    auto codes = codes_from(repository.listProblems(filter));
    REQUIRE(codes == std::vector<std::string>{graph_code});

    filter.tag = std::to_string(tag_id);
    codes = codes_from(repository.listProblems(filter));
    REQUIRE(codes == std::vector<std::string>{graph_code});

    filter.tag = "missing-tag," + greedy_tag;
    codes = codes_from(repository.listProblems(filter));
    REQUIRE(codes == std::vector<std::string>{greedy_code});
}
