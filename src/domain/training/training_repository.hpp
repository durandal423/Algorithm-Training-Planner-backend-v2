#pragma once

#include "db/connection_pool.hpp"
#include "domain/training/training_models.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace training {

namespace detail {

inline std::string trim_copy(std::string text) {
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

} // namespace detail

template <typename Row>
TrainingGoal goal_from_row(const Row& row) {
    TrainingGoal goal;
    goal.id = row["id"].template as<long long>();
    goal.user_id = row["user_id"].template as<long long>();
    goal.name = row["name"].template as<std::string>();
    goal.target_count = row["target_count"].template as<int>();
    goal.time_budget_minutes = row["time_budget_minutes"].template as<int>();
    goal.difficulty_min = row["difficulty_min"].template get<int>();
    goal.difficulty_max = row["difficulty_max"].template get<int>();
    goal.prefer_wrong_problems = row["prefer_wrong_problems"].template as<bool>();
    goal.prefer_weak_tags = row["prefer_weak_tags"].template as<bool>();
    goal.difficulty_weight = row["difficulty_weight"].template as<int>();
    goal.tag_coverage_weight = row["tag_coverage_weight"].template as<int>();
    goal.wrong_problem_weight = row["wrong_problem_weight"].template as<int>();
    goal.weak_tag_weight = row["weak_tag_weight"].template as<int>();
    goal.estimated_time_weight = row["estimated_time_weight"].template as<int>();
    goal.created_at = row["created_at"].template as<std::string>("");
    return goal;
}

template <typename Row>
TrainingPlan plan_from_row(const Row& row) {
    TrainingPlan plan;
    plan.id = row["id"].template as<long long>();
    plan.user_id = row["user_id"].template as<long long>();
    plan.goal_id = row["goal_id"].template get<long long>();
    plan.name = row["name"].template as<std::string>();
    plan.status = row["status"].template as<std::string>();
    plan.candidate_count = row["candidate_count"].template as<int>();
    plan.selected_count = row["selected_count"].template as<int>();
    plan.total_estimated_minutes = row["total_estimated_minutes"].template as<int>();
    plan.algorithm_summary = row["algorithm_summary"].template as<std::string>("");
    plan.created_at = row["created_at"].template as<std::string>("");
    return plan;
}

template <typename Row>
TrainingPlanItem item_from_row(const Row& row) {
    TrainingPlanItem item;
    item.id = row["id"].template as<long long>();
    item.plan_id = row["plan_id"].template as<long long>();
    item.problem_id = row["problem_id"].template as<long long>();
    item.order_index = row["order_index"].template as<int>();
    item.estimated_minutes = row["estimated_minutes"].template as<int>();
    item.score = row["score"].template as<int>();
    item.selected_reason = row["selected_reason"].template as<std::string>("");
    item.status = row["status"].template as<std::string>();
    item.last_submission_id = row["last_submission_id"].template get<long long>();
    item.last_verdict = row["last_verdict"].template get<std::string>();
    item.updated_at = row["updated_at"].template as<std::string>("");

    std::istringstream input{row["tag_names"].template as<std::string>("")};
    std::string tag;
    while (std::getline(input, tag, ',')) {
        tag = detail::trim_copy(tag);
        if (!tag.empty()) {
            item.tag_names.push_back(std::move(tag));
        }
    }
    return item;
}

template <typename Row>
TrainingProblemCandidate candidate_from_row(const Row& row) {
    TrainingProblemCandidate candidate;
    candidate.id = row["id"].template as<long long>();
    candidate.code = row["code"].template as<std::string>();
    candidate.title = row["title"].template as<std::string>();
    candidate.difficulty = row["difficulty"].template as<int>();
    candidate.estimated_minutes = row["estimated_minutes"].template as<int>();
    candidate.matched_tag_count = row["matched_tag_count"].template as<int>();
    candidate.wrong_attempt_count = row["wrong_attempt_count"].template as<int>();

    std::istringstream input{row["tag_names"].template as<std::string>("")};
    std::string tag;
    while (std::getline(input, tag, ',')) {
        tag = detail::trim_copy(tag);
        if (!tag.empty()) {
            candidate.tag_names.push_back(std::move(tag));
        }
    }
    return candidate;
}

class TrainingRepository final {
public:
    explicit TrainingRepository(ConnectionPool& pool);

    long long createGoal(const TrainingGoal& goal) const;
    std::optional<TrainingGoal> getGoal(long long id, long long user_id) const;
    std::vector<TrainingGoal> listGoals(long long user_id) const;
    bool updateGoal(long long id, const TrainingGoal& goal) const;
    bool deleteGoal(long long id, long long user_id) const;

    long long createPlan(const TrainingPlan& plan, const std::vector<TrainingPlanItem>& items) const;
    std::optional<TrainingPlan> getPlan(long long id, long long user_id) const;
    std::vector<TrainingPlan> listPlans(long long user_id) const;
    std::vector<TrainingPlanItem> listPlanItems(long long plan_id) const;
    std::vector<TrainingProblemCandidate> listProblemCandidates(
        const TrainingGoal& goal,
        bool include_hidden
    ) const;
    bool updatePlanItemFromSubmission(long long submission_id, const std::string& verdict) const;

    long long createRecord(const TrainingRecord& record) const;

private:
    ConnectionPool& pool_;

    long long upsertTag(pqxx::transaction_base& tx, const std::string& name) const;
    void replaceGoalTags(
        pqxx::transaction_base& tx,
        long long goal_id,
        const std::vector<std::string>& names
    ) const;
    std::vector<std::string> loadGoalTags(pqxx::transaction_base& tx, long long goal_id) const;
};

} // namespace training
