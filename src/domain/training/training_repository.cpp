#include "domain/training/training_repository.hpp"

#include <pqxx/pqxx>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace training {
namespace {

std::optional<int> optional_int(const pqxx::field_ref field) {
    if (field.is_null()) {
        return std::nullopt;
    }
    return field.as<int>();
}

std::optional<long long> optional_long(const pqxx::field_ref field) {
    if (field.is_null()) {
        return std::nullopt;
    }
    return field.as<long long>();
}

std::optional<std::string> optional_string(const pqxx::field_ref field) {
    if (field.is_null()) {
        return std::nullopt;
    }
    return field.as<std::string>();
}

std::string string_or_empty(const pqxx::field_ref field) {
    if (field.is_null()) {
        return {};
    }
    return field.as<std::string>();
}

std::string trim_copy(std::string text) {
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string{first, last};
}

TrainingGoal goal_from_row(const pqxx::row& row) {
    TrainingGoal goal;
    goal.id = row["id"].as<long long>();
    goal.user_id = row["user_id"].as<long long>();
    goal.name = row["name"].as<std::string>();
    goal.target_count = row["target_count"].as<int>();
    goal.time_budget_minutes = row["time_budget_minutes"].as<int>();
    goal.difficulty_min = optional_int(row["difficulty_min"]);
    goal.difficulty_max = optional_int(row["difficulty_max"]);
    goal.prefer_wrong_problems = row["prefer_wrong_problems"].as<bool>();
    goal.prefer_weak_tags = row["prefer_weak_tags"].as<bool>();
    goal.difficulty_weight = row["difficulty_weight"].as<int>();
    goal.tag_coverage_weight = row["tag_coverage_weight"].as<int>();
    goal.wrong_problem_weight = row["wrong_problem_weight"].as<int>();
    goal.weak_tag_weight = row["weak_tag_weight"].as<int>();
    goal.estimated_time_weight = row["estimated_time_weight"].as<int>();
    goal.created_at = string_or_empty(row["created_at"]);
    return goal;
}

TrainingGoal goal_from_row(const pqxx::row_ref& row) {
    return goal_from_row(pqxx::row{row});
}

TrainingPlan plan_from_row(const pqxx::row& row) {
    TrainingPlan plan;
    plan.id = row["id"].as<long long>();
    plan.user_id = row["user_id"].as<long long>();
    plan.goal_id = optional_long(row["goal_id"]);
    plan.name = row["name"].as<std::string>();
    plan.status = row["status"].as<std::string>();
    plan.candidate_count = row["candidate_count"].as<int>();
    plan.selected_count = row["selected_count"].as<int>();
    plan.total_estimated_minutes = row["total_estimated_minutes"].as<int>();
    plan.algorithm_summary = string_or_empty(row["algorithm_summary"]);
    plan.created_at = string_or_empty(row["created_at"]);
    return plan;
}

TrainingPlan plan_from_row(const pqxx::row_ref& row) {
    return plan_from_row(pqxx::row{row});
}

TrainingPlanItem item_from_row(const pqxx::row& row) {
    TrainingPlanItem item;
    item.id = row["id"].as<long long>();
    item.plan_id = row["plan_id"].as<long long>();
    item.problem_id = row["problem_id"].as<long long>();
    item.order_index = row["order_index"].as<int>();
    item.estimated_minutes = row["estimated_minutes"].as<int>();
    item.score = row["score"].as<int>();
    item.selected_reason = string_or_empty(row["selected_reason"]);
    item.status = row["status"].as<std::string>();
    item.last_submission_id = optional_long(row["last_submission_id"]);
    item.last_verdict = optional_string(row["last_verdict"]);
    item.updated_at = string_or_empty(row["updated_at"]);
    return item;
}

TrainingPlanItem item_from_row(const pqxx::row_ref& row) {
    return item_from_row(pqxx::row{row});
}

TrainingProblemCandidate candidate_from_row(const pqxx::row& row) {
    TrainingProblemCandidate candidate;
    candidate.id = row["id"].as<long long>();
    candidate.code = row["code"].as<std::string>();
    candidate.title = row["title"].as<std::string>();
    candidate.difficulty = row["difficulty"].as<int>();
    candidate.estimated_minutes = row["estimated_minutes"].as<int>();
    candidate.matched_tag_count = row["matched_tag_count"].as<int>();
    candidate.wrong_attempt_count = row["wrong_attempt_count"].as<int>();

    std::istringstream input{string_or_empty(row["tag_names"])};
    std::string tag;
    while (std::getline(input, tag, ',')) {
        tag = trim_copy(tag);
        if (!tag.empty()) {
            candidate.tag_names.push_back(std::move(tag));
        }
    }
    return candidate;
}

TrainingProblemCandidate candidate_from_row(const pqxx::row_ref& row) {
    return candidate_from_row(pqxx::row{row});
}

constexpr auto kGoalSelect = R"SQL(
    SELECT id, user_id, name, target_count, time_budget_minutes,
           difficulty_min, difficulty_max, prefer_wrong_problems, prefer_weak_tags,
           difficulty_weight, tag_coverage_weight, wrong_problem_weight,
           weak_tag_weight, estimated_time_weight, created_at
    FROM training_goals
)SQL";

constexpr auto kPlanSelect = R"SQL(
    SELECT id, user_id, goal_id, name, status, candidate_count,
           selected_count, total_estimated_minutes, algorithm_summary, created_at
    FROM training_plans
)SQL";

} // namespace

TrainingRepository::TrainingRepository(ConnectionPool& pool)
    : pool_(pool) {}

long long TrainingRepository::createGoal(const TrainingGoal& goal) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto id = tx.exec(
        R"SQL(
            INSERT INTO training_goals(
                user_id, name, target_count, time_budget_minutes,
                difficulty_min, difficulty_max, prefer_wrong_problems, prefer_weak_tags,
                difficulty_weight, tag_coverage_weight, wrong_problem_weight,
                weak_tag_weight, estimated_time_weight
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13)
            RETURNING id
        )SQL",
        pqxx::params{
            goal.user_id,
            goal.name,
            goal.target_count,
            goal.time_budget_minutes,
            goal.difficulty_min,
            goal.difficulty_max,
            goal.prefer_wrong_problems,
            goal.prefer_weak_tags,
            goal.difficulty_weight,
            goal.tag_coverage_weight,
            goal.wrong_problem_weight,
            goal.weak_tag_weight,
            goal.estimated_time_weight
        }
    ).one_field().as<long long>();
    replaceGoalTags(tx, id, goal.tag_names);
    tx.commit();
    return id;
}

std::optional<TrainingGoal> TrainingRepository::getGoal(long long id, long long user_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        std::string{kGoalSelect} + R"SQL(
            WHERE id = $1 AND user_id = $2
        )SQL",
        pqxx::params{id, user_id}
    ).opt_row();
    if (!row) {
        tx.commit();
        return std::nullopt;
    }
    auto goal = goal_from_row(*row);
    goal.tag_names = loadGoalTags(tx, goal.id);
    tx.commit();
    return goal;
}

std::vector<TrainingGoal> TrainingRepository::listGoals(long long user_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<TrainingGoal> goals;
    const auto rows = tx.exec(
        std::string{kGoalSelect} + R"SQL(
            WHERE user_id = $1
            ORDER BY created_at DESC, id DESC
        )SQL",
        pqxx::params{user_id}
    );
    for (const auto& row : rows) {
        auto goal = goal_from_row(row);
        goal.tag_names = loadGoalTags(tx, goal.id);
        goals.push_back(std::move(goal));
    }
    tx.commit();
    return goals;
}

bool TrainingRepository::updateGoal(long long id, const TrainingGoal& goal) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto result = tx.exec(
        R"SQL(
            UPDATE training_goals
            SET name = $3,
                target_count = $4,
                time_budget_minutes = $5,
                difficulty_min = $6,
                difficulty_max = $7,
                prefer_wrong_problems = $8,
                prefer_weak_tags = $9,
                difficulty_weight = $10,
                tag_coverage_weight = $11,
                wrong_problem_weight = $12,
                weak_tag_weight = $13,
                estimated_time_weight = $14
            WHERE id = $1 AND user_id = $2
        )SQL",
        pqxx::params{
            id,
            goal.user_id,
            goal.name,
            goal.target_count,
            goal.time_budget_minutes,
            goal.difficulty_min,
            goal.difficulty_max,
            goal.prefer_wrong_problems,
            goal.prefer_weak_tags,
            goal.difficulty_weight,
            goal.tag_coverage_weight,
            goal.wrong_problem_weight,
            goal.weak_tag_weight,
            goal.estimated_time_weight
        }
    );
    if (result.affected_rows() > 0) {
        replaceGoalTags(tx, id, goal.tag_names);
    }
    tx.commit();
    return result.affected_rows() > 0;
}

bool TrainingRepository::deleteGoal(long long id, long long user_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto result = tx.exec(
        "DELETE FROM training_goals WHERE id = $1 AND user_id = $2",
        pqxx::params{id, user_id}
    );
    tx.commit();
    return result.affected_rows() > 0;
}

long long TrainingRepository::createPlan(
    const TrainingPlan& plan,
    const std::vector<TrainingPlanItem>& items
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto plan_id = tx.exec(
        R"SQL(
            INSERT INTO training_plans(
                user_id, goal_id, name, status, candidate_count,
                selected_count, total_estimated_minutes, algorithm_summary
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
            RETURNING id
        )SQL",
        pqxx::params{
            plan.user_id,
            plan.goal_id,
            plan.name,
            plan.status,
            plan.candidate_count,
            plan.selected_count,
            plan.total_estimated_minutes,
            plan.algorithm_summary
        }
    ).one_field().as<long long>();

    for (const auto& item : items) {
        tx.exec(
            R"SQL(
                INSERT INTO training_plan_items(
                    plan_id, problem_id, order_index, estimated_minutes,
                    score, selected_reason, status
                )
                VALUES ($1, $2, $3, $4, $5, $6, $7)
            )SQL",
            pqxx::params{
                plan_id,
                item.problem_id,
                item.order_index,
                item.estimated_minutes,
                item.score,
                item.selected_reason,
                item.status
            }
        ).no_rows();
    }

    tx.commit();
    return plan_id;
}

std::optional<TrainingPlan> TrainingRepository::getPlan(long long id, long long user_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        std::string{kPlanSelect} + R"SQL(
            WHERE id = $1 AND user_id = $2
        )SQL",
        pqxx::params{id, user_id}
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return plan_from_row(*row);
}

std::vector<TrainingPlan> TrainingRepository::listPlans(long long user_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<TrainingPlan> plans;
    const auto rows = tx.exec(
        std::string{kPlanSelect} + R"SQL(
            WHERE user_id = $1
            ORDER BY created_at DESC, id DESC
        )SQL",
        pqxx::params{user_id}
    );
    for (const auto& row : rows) {
        plans.push_back(plan_from_row(row));
    }
    tx.commit();
    return plans;
}

std::vector<TrainingPlanItem> TrainingRepository::listPlanItems(long long plan_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<TrainingPlanItem> items;
    const auto rows = tx.exec(
        R"SQL(
            SELECT id, plan_id, problem_id, order_index, estimated_minutes, score,
                   selected_reason, status, last_submission_id, last_verdict, updated_at
            FROM training_plan_items
            WHERE plan_id = $1
            ORDER BY order_index, id
        )SQL",
        pqxx::params{plan_id}
    );
    for (const auto& row : rows) {
        items.push_back(item_from_row(row));
    }
    tx.commit();
    return items;
}

std::vector<TrainingProblemCandidate> TrainingRepository::listProblemCandidates(
    const TrainingGoal& goal,
    bool include_hidden
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<TrainingProblemCandidate> candidates;
    std::string goal_tags_text;
    for (const auto& tag : goal.tag_names) {
        if (!goal_tags_text.empty()) {
            goal_tags_text.push_back('\n');
        }
        goal_tags_text += tag;
    }
    const auto rows = tx.exec(
        R"SQL(
            WITH goal_tags AS (
                SELECT trim(name) AS name
                FROM unnest(string_to_array($6, E'\n')) AS name
                WHERE trim(name) <> ''
            ),
            tagged AS (
                SELECT p.id,
                       p.code,
                       p.title,
                       p.difficulty,
                       p.estimated_minutes,
                       COALESCE(string_agg(DISTINCT t.name, ',' ORDER BY t.name), '') AS tag_names,
                       COUNT(DISTINCT gt.name)::integer AS matched_tag_count,
                       COUNT(DISTINCT s.id) FILTER (
                           WHERE s.user_id = $1 AND s.verdict IS NOT NULL AND s.verdict <> 'AC'
                       )::integer AS wrong_attempt_count
                FROM problems p
                LEFT JOIN problem_tags pt ON pt.problem_id = p.id
                LEFT JOIN tags t ON t.id = pt.tag_id
                LEFT JOIN goal_tags gt ON lower(gt.name) = lower(t.name)
                LEFT JOIN submissions s ON s.problem_id = p.id
                WHERE ($2 OR p.visibility = 'public')
                  AND ($3::integer IS NULL OR p.difficulty >= $3)
                  AND ($4::integer IS NULL OR p.difficulty <= $4)
                GROUP BY p.id, p.code, p.title, p.difficulty, p.estimated_minutes
            )
            SELECT id, code, title, difficulty, estimated_minutes,
                   tag_names, matched_tag_count, wrong_attempt_count
            FROM tagged
            WHERE ($5 = 0 OR matched_tag_count > 0)
            ORDER BY difficulty ASC, estimated_minutes ASC, id ASC
        )SQL",
        pqxx::params{
            goal.user_id,
            include_hidden,
            goal.difficulty_min,
            goal.difficulty_max,
            static_cast<int>(goal.tag_names.size()),
            goal_tags_text
        }
    );
    for (const auto& row : rows) {
        candidates.push_back(candidate_from_row(row));
    }
    tx.commit();
    return candidates;
}

bool TrainingRepository::updatePlanItemFromSubmission(
    long long submission_id,
    const std::string& verdict
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto result = tx.exec(
        R"SQL(
            UPDATE training_plan_items tpi
            SET last_submission_id = s.id,
                last_verdict = $2::varchar,
                status = CASE WHEN $2::text = 'AC' THEN 'completed' ELSE 'failed' END
            FROM submissions s
            WHERE s.id = $1
              AND tpi.problem_id = s.problem_id
              AND tpi.plan_id IN (
                    SELECT plan_id
                    FROM training_records
                    WHERE submission_id = s.id
                    UNION
                    SELECT id
                    FROM training_plans
                    WHERE user_id = s.user_id
              )
        )SQL",
        pqxx::params{submission_id, verdict}
    );
    tx.commit();
    return result.affected_rows() > 0;
}

long long TrainingRepository::createRecord(const TrainingRecord& record) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto id = tx.exec(
        R"SQL(
            INSERT INTO training_records(
                user_id, plan_id, plan_item_id, problem_id, submission_id,
                is_finished, is_first_try_ac, actual_minutes, error_type, review_note
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)
            RETURNING id
        )SQL",
        pqxx::params{
            record.user_id,
            record.plan_id,
            record.plan_item_id,
            record.problem_id,
            record.submission_id,
            record.is_finished,
            record.is_first_try_ac,
            record.actual_minutes,
            record.error_type,
            record.review_note
        }
    ).one_field().as<long long>();
    tx.commit();
    return id;
}

long long TrainingRepository::upsertTag(pqxx::transaction_base& tx, const std::string& name) const {
    const auto clean_name = trim_copy(name);
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

void TrainingRepository::replaceGoalTags(
    pqxx::transaction_base& tx,
    long long goal_id,
    const std::vector<std::string>& names
) const {
    tx.exec(
        "DELETE FROM training_goal_tags WHERE goal_id = $1",
        pqxx::params{goal_id}
    ).no_rows();
    for (const auto& name : names) {
        const auto tag_id = upsertTag(tx, name);
        if (tag_id == 0) {
            continue;
        }
        tx.exec(
            R"SQL(
                INSERT INTO training_goal_tags(goal_id, tag_id)
                VALUES ($1, $2)
                ON CONFLICT DO NOTHING
            )SQL",
            pqxx::params{goal_id, tag_id}
        ).no_rows();
    }
}

std::vector<std::string> TrainingRepository::loadGoalTags(
    pqxx::transaction_base& tx,
    long long goal_id
) const {
    std::vector<std::string> tags;
    const auto rows = tx.exec(
        R"SQL(
            SELECT t.name
            FROM tags t
            JOIN training_goal_tags tgt ON tgt.tag_id = t.id
            WHERE tgt.goal_id = $1
            ORDER BY t.name
        )SQL",
        pqxx::params{goal_id}
    );
    for (const auto& row : rows) {
        tags.push_back(row["name"].as<std::string>());
    }
    return tags;
}

} // namespace training
