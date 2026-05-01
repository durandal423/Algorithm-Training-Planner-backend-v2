#pragma once

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

} // namespace training
