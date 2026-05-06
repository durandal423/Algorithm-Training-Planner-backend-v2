#include "domain/problem/problem_repository.hpp"
#include "api/http_json.hpp"

#include <pqxx/pqxx>

#include <algorithm>
#include <unordered_map>

namespace problem {
namespace {

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
    const auto result= tx.exec(
        R"SQL(
            WITH tag_filter AS (
                SELECT trim(value) AS value
                FROM unnest(string_to_array($3, ',')) AS value
                WHERE trim(value) <> ''
            )
        )SQL" + std::string{kProblemSummarySelect} + R"SQL(
            WHERE ($1 OR visibility = 'public')
              AND ($2 = '' OR code ILIKE '%' || $2 || '%' OR title ILIKE '%' || $2 || '%')
              AND (NOT EXISTS (SELECT 1 FROM tag_filter) OR EXISTS (
                    SELECT 1
                    FROM problem_tags pt
                    JOIN tags t ON t.id = pt.tag_id
                    JOIN tag_filter tf
                      ON lower(t.name) = lower(tf.value)
                      OR t.id::text = tf.value
                    WHERE pt.problem_id = problems.id
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
    problems.reserve(result.size());
    for (const pqxx::row_ref row : result) {
        problems.push_back(problem_summary_from_row(row));
    }
    loadTagsForProblems(tx, problems);
    tx.commit();
    return problems;
}

long long ProblemRepository::countProblems(bool include_hidden) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto count = tx.exec(
        R"SQL(
            SELECT COUNT(*)::bigint
            FROM problems
            WHERE ($1 OR visibility = 'public')
        )SQL",
        pqxx::params{include_hidden}
    ).one_field().as<long long>();
    tx.commit();
    return count;
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
    const auto result= tx.exec(R"SQL(
        SELECT id, name, description
        FROM tags
        ORDER BY name
    )SQL");
    for (const pqxx::row_ref row : result) {
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
    const auto result= tx.exec(
        R"SQL(
            SELECT t.id, t.name, t.description
            FROM tags t
            JOIN problem_tags pt ON pt.tag_id = t.id
            WHERE pt.problem_id = $1
            ORDER BY t.name
        )SQL",
        pqxx::params{problem_id}
    );
    for (const pqxx::row_ref row : result) {
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

    const auto result= tx.exec(
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
    for (const pqxx::row_ref row : result) {
        const auto problem_id = row["problem_id"].template as<long long>();
        const auto found = problems_by_id.find(problem_id);
        if (found == problems_by_id.end()) {
            continue;
        }
        found->second->tags.push_back(tag_from_row(row));
    }
}

} // namespace problem
