#include "domain/problem/problem_repository.hpp"
#include "api/http_json.hpp"

#include <pqxx/pqxx>

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace problem {
namespace {

Problem problem_from_row(const auto& row) {
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

ProblemSummary problem_summary_from_row(const auto& row) {
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

Tag tag_from_row(const auto& row) {
    Tag tag;
    tag.id = row["id"].template as<long long>();
    tag.name = row["name"].template as<std::string>();
    tag.description = row["description"].template as<std::string>("");
    return tag;
}

constexpr auto kProblemSelect = R"SQL(
    SELECT id, code, title, source_platform, source_url, difficulty, estimated_minutes,
           description, input_format, output_format, sample_input, sample_output,
           hint, visibility, created_by, created_at, updated_at
    FROM problems
)SQL";

constexpr auto kProblemSummarySelect = R"SQL(
    SELECT id, code, title, source_platform, source_url, difficulty, estimated_minutes,
           visibility, created_by, created_at, updated_at
    FROM problems
)SQL";

} // namespace

ProblemRepository::ProblemRepository(ConnectionPool& pool)
    : pool_(pool) {}

std::vector<ProblemSummary> ProblemRepository::listProblems(const ProblemFilter& filter) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<ProblemSummary> problems;

    const int limit = std::clamp(filter.limit, 1, 200);
    const int offset = std::max(0, filter.offset);
    const auto rows = tx.exec(
        std::string{kProblemSummarySelect} + R"SQL(
            WHERE ($1 OR visibility = 'public')
              AND ($2 = '' OR code ILIKE '%' || $2 || '%' OR title ILIKE '%' || $2 || '%')
              AND ($3 = '' OR EXISTS (
                    SELECT 1
                    FROM problem_tags pt
                    JOIN tags t ON t.id = pt.tag_id
                    WHERE pt.problem_id = problems.id AND lower(t.name) = lower($3)
              ))
              AND ($4::integer IS NULL OR difficulty >= $4)
              AND ($5::integer IS NULL OR difficulty <= $5)
            ORDER BY id DESC
            LIMIT $6 OFFSET $7
        )SQL",
        pqxx::params{
            filter.include_hidden,
            filter.keyword,
            filter.tag,
            filter.difficulty_min,
            filter.difficulty_max,
            limit,
            offset
        }
    );
    problems.reserve(rows.size());
    for (const auto& row : rows) {
        problems.push_back(problem_summary_from_row(row));
    }
    loadTagsForProblems(tx, problems);
    tx.commit();
    return problems;
}

std::optional<Problem> ProblemRepository::getProblem(long long id, bool include_hidden) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        std::string{kProblemSelect} + R"SQL(
            WHERE id = $1
              AND ($2 OR visibility = 'public')
        )SQL",
        pqxx::params{id, include_hidden}
    ).opt_row();
    if (!row) {
        tx.commit();
        return std::nullopt;
    }
    auto problem = problem_from_row(row.value());
    problem.tags = loadTags(tx, problem.id);
    tx.commit();
    return problem;
}

long long ProblemRepository::createProblem(
    const Problem& problem,
    const std::vector<std::string>& tag_names
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto id = tx.exec(
        R"SQL(
            INSERT INTO problems(
                code, title, source_platform, source_url, difficulty, estimated_minutes,
                description, input_format, output_format, sample_input, sample_output,
                hint, visibility, created_by
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14)
            RETURNING id
        )SQL",
        pqxx::params{
            problem.code,
            problem.title,
            problem.source_platform,
            problem.source_url,
            problem.difficulty,
            problem.estimated_minutes,
            problem.description,
            problem.input_format,
            problem.output_format,
            problem.sample_input,
            problem.sample_output,
            problem.hint,
            problem.visibility,
            problem.created_by
        }
    ).one_field().as<long long>();
    replaceProblemTags(tx, id, tag_names);
    tx.commit();
    return id;
}

bool ProblemRepository::updateProblem(
    long long id,
    const Problem& problem,
    const std::vector<std::string>& tag_names
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto result = tx.exec(
        R"SQL(
            UPDATE problems
            SET code = $2,
                title = $3,
                source_platform = $4,
                source_url = $5,
                difficulty = $6,
                estimated_minutes = $7,
                description = $8,
                input_format = $9,
                output_format = $10,
                sample_input = $11,
                sample_output = $12,
                hint = $13,
                visibility = $14
            WHERE id = $1
        )SQL",
        pqxx::params{
            id,
            problem.code,
            problem.title,
            problem.source_platform,
            problem.source_url,
            problem.difficulty,
            problem.estimated_minutes,
            problem.description,
            problem.input_format,
            problem.output_format,
            problem.sample_input,
            problem.sample_output,
            problem.hint,
            problem.visibility
        }
    );
    if (result.affected_rows() > 0) {
        replaceProblemTags(tx, id, tag_names);
    }
    tx.commit();
    return result.affected_rows() > 0;
}

bool ProblemRepository::deleteProblem(long long id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto result = tx.exec(
        "DELETE FROM problems WHERE id = $1",
        pqxx::params{id}
    );
    tx.commit();
    return result.affected_rows() > 0;
}

std::vector<Tag> ProblemRepository::listTags() const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<Tag> tags;
    const auto rows = tx.exec(R"SQL(
        SELECT id, name, description
        FROM tags
        ORDER BY name
    )SQL");
    for (const auto& row : rows) {
        tags.push_back(tag_from_row(row));
    }
    tx.commit();
    return tags;
}

long long ProblemRepository::upsertTag(pqxx::transaction_base& tx, const std::string& name) const {
    const auto clean_name = api::trim_copy(name);
    if (clean_name.empty()) {
        return 0;
    }
    return tx.exec(
        R"SQL(
            INSERT INTO tags(name)
            VALUES ($1)
            ON CONFLICT (name) DO UPDATE SET name = EXCLUDED.name
            RETURNING id
        )SQL",
        pqxx::params{clean_name}
    ).one_field().as<long long>();
}

void ProblemRepository::replaceProblemTags(
    pqxx::transaction_base& tx,
    long long problem_id,
    const std::vector<std::string>& names
) const {
    tx.exec(
        "DELETE FROM problem_tags WHERE problem_id = $1",
        pqxx::params{problem_id}
    ).no_rows();
    for (const auto& name : names) {
        const auto tag_id = upsertTag(tx, name);
        if (tag_id == 0) {
            continue;
        }
        tx.exec(
            R"SQL(
                INSERT INTO problem_tags(problem_id, tag_id)
                VALUES ($1, $2)
                ON CONFLICT DO NOTHING
            )SQL",
            pqxx::params{problem_id, tag_id}
        ).no_rows();
    }
}

std::vector<Tag> ProblemRepository::loadTags(pqxx::transaction_base& tx, long long problem_id) const {
    std::vector<Tag> tags;
    const auto rows = tx.exec(
        R"SQL(
            SELECT t.id, t.name, t.description
            FROM tags t
            JOIN problem_tags pt ON pt.tag_id = t.id
            WHERE pt.problem_id = $1
            ORDER BY t.name
        )SQL",
        pqxx::params{problem_id}
    );
    for (const auto& row : rows) {
        tags.push_back(tag_from_row(row));
    }
    return tags;
}

void ProblemRepository::loadTagsForProblems(
    pqxx::transaction_base& tx,
    std::vector<ProblemSummary>& problems
) const {
    if (problems.empty()) {
        return;
    }

    std::unordered_map<long long, ProblemSummary*> problems_by_id;
    problems_by_id.reserve(problems.size());
    pqxx::params params;
    params.reserve(problems.size());
    pqxx::placeholders placeholder;
    std::string placeholders;
    for (auto& problem : problems) {
        problems_by_id.emplace(problem.id, &problem);
        params.append(problem.id);
        if (!placeholders.empty()) {
            placeholders += ", ";
        }
        placeholders += placeholder.get();
        placeholder.next();
    }

    const auto rows = tx.exec(
        R"SQL(
            SELECT pt.problem_id, t.id, t.name, t.description
            FROM tags t
            JOIN problem_tags pt ON pt.tag_id = t.id
            WHERE pt.problem_id IN (
        )SQL" + placeholders + R"SQL(
            )
            ORDER BY pt.problem_id, t.name
        )SQL",
        params
    );
    for (const auto& row : rows) {
        const auto problem_id = row["problem_id"].template as<long long>();
        const auto found = problems_by_id.find(problem_id);
        if (found == problems_by_id.end()) {
            continue;
        }
        found->second->tags.push_back(tag_from_row(row));
    }
}

} // namespace problem
