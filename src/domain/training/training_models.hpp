#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace training {

struct TrainingGoal {
    long long id{};
    long long user_id{};
    std::string name;
    int target_count{10};
    int time_budget_minutes{180};
    std::optional<int> difficulty_min;
    std::optional<int> difficulty_max;
    bool prefer_wrong_problems{true};
    bool prefer_weak_tags{true};
    int difficulty_weight{10};
    int tag_coverage_weight{20};
    int wrong_problem_weight{15};
    int weak_tag_weight{15};
    int estimated_time_weight{1};
    std::string created_at;
    std::vector<std::string> tag_names;
};

inline void to_json(nlohmann::json& json, const TrainingGoal& goal) {
    json = {
        {"id", goal.id},
        {"userId", goal.user_id},
        {"name", goal.name},
        {"targetCount", goal.target_count},
        {"timeBudgetMinutes", goal.time_budget_minutes},
        {"difficultyMin", goal.difficulty_min},
        {"difficultyMax", goal.difficulty_max},
        {"preferWrongProblems", goal.prefer_wrong_problems},
        {"preferWeakTags", goal.prefer_weak_tags},
        {"difficultyWeight", goal.difficulty_weight},
        {"tagCoverageWeight", goal.tag_coverage_weight},
        {"wrongProblemWeight", goal.wrong_problem_weight},
        {"weakTagWeight", goal.weak_tag_weight},
        {"estimatedTimeWeight", goal.estimated_time_weight},
        {"createdAt", goal.created_at},
        {"tags", goal.tag_names}
    };
}

struct TrainingPlan {
    long long id{};
    long long user_id{};
    std::optional<long long> goal_id;
    std::string name;
    std::string status{"draft"};
    int candidate_count{};
    int selected_count{};
    int total_estimated_minutes{};
    std::string algorithm_summary;
    std::string created_at;
};

inline void to_json(nlohmann::json& json, const TrainingPlan& plan) {
    json = {
        {"id", plan.id},
        {"userId", plan.user_id},
        {"goalId", plan.goal_id},
        {"name", plan.name},
        {"status", plan.status},
        {"candidateCount", plan.candidate_count},
        {"selectedCount", plan.selected_count},
        {"totalEstimatedMinutes", plan.total_estimated_minutes},
        {"algorithmSummary", plan.algorithm_summary},
        {"createdAt", plan.created_at}
    };
}

struct TrainingPlanItem {
    long long id{};
    long long plan_id{};
    long long problem_id{};
    int order_index{};
    int estimated_minutes{};
    int score{};
    std::string selected_reason;
    std::string status{"not_started"};
    std::optional<long long> last_submission_id;
    std::optional<std::string> last_verdict;
    std::string updated_at;
};

inline void to_json(nlohmann::json& json, const TrainingPlanItem& item) {
    json = {
        {"id", item.id},
        {"planId", item.plan_id},
        {"problemId", item.problem_id},
        {"orderIndex", item.order_index},
        {"estimatedMinutes", item.estimated_minutes},
        {"score", item.score},
        {"selectedReason", item.selected_reason},
        {"status", item.status},
        {"lastSubmissionId", item.last_submission_id},
        {"lastVerdict", item.last_verdict},
        {"updatedAt", item.updated_at}
    };
}

struct TrainingRecord {
    long long id{};
    long long user_id{};
    std::optional<long long> plan_id;
    std::optional<long long> plan_item_id;
    long long problem_id{};
    std::optional<long long> submission_id;
    bool is_finished{};
    bool is_first_try_ac{};
    std::optional<int> actual_minutes;
    std::string error_type;
    std::string review_note;
    std::string practiced_at;
};

struct TrainingProblemCandidate {
    long long id{};
    std::string code;
    std::string title;
    int difficulty{};
    int estimated_minutes{};
    int matched_tag_count{};
    int wrong_attempt_count{};
    std::vector<std::string> tag_names;
};

struct StableDifficultyWindow {
    int left_index{-1};
    int right_index{-1};
    int left_difficulty{};
    int right_difficulty{};
    int difficulty_span{};
    int wrong_problem_count{};
    double coverage_ratio{};
    std::vector<std::string> covered_tags;
    std::vector<std::string> uncovered_tags;
    std::vector<TrainingProblemCandidate> candidates;
};

struct SelectedTrainingProblem {
    TrainingProblemCandidate problem;
    int score{};
    std::vector<std::string> covered_target_tags;
    std::string selected_reason;
};

struct OptimizedTrainingPlan {
    std::vector<SelectedTrainingProblem> items;
    int total_estimated_minutes{};
    int total_score{};
    std::vector<std::string> covered_tags;
    std::vector<std::string> uncovered_tags;
};

} // namespace training
