#include "algorithms/training_plan_algorithms.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

training::TrainingProblemCandidate candidate(
    long long id,
    int difficulty,
    int minutes,
    std::vector<std::string> tags,
    int wrong_attempt_count = 0
) {
    return training::TrainingProblemCandidate{
        .id = id,
        .code = "T" + std::to_string(id),
        .title = "Problem " + std::to_string(id),
        .difficulty = difficulty,
        .estimated_minutes = minutes,
        .matched_tag_count = 0,
        .wrong_attempt_count = wrong_attempt_count,
        .tag_names = std::move(tags)
    };
}

training::TrainingGoal goal() {
    training::TrainingGoal item;
    item.target_count = 2;
    item.time_budget_minutes = 50;
    item.difficulty_min = 900;
    item.difficulty_max = 1800;
    item.tag_names = {"动态规划", "双指针"};
    item.prefer_wrong_problems = true;
    item.prefer_weak_tags = true;
    return item;
}

} // namespace

TEST_CASE("selectStableDifficultyWindow uses a stable two-pointer difficulty window") {
    const std::vector<training::TrainingProblemCandidate> candidates = {
        candidate(1, 900, 15, {"排序"}),
        candidate(2, 1000, 20, {"动态规划"}),
        candidate(3, 1050, 20, {"双指针"}),
        candidate(4, 1100, 20, {"贪心"}),
        candidate(5, 1700, 40, {"图论"})
    };

    auto selected_goal = goal();
    selected_goal.target_count = 1;
    const auto window = training::selectStableDifficultyWindow(candidates, selected_goal);

    REQUIRE(window.left_difficulty == 1000);
    REQUIRE(window.right_difficulty == 1100);
    REQUIRE(window.difficulty_span == 100);
    REQUIRE(window.candidates.size() == 3);
    REQUIRE(window.covered_tags.size() == 2);
}

TEST_CASE("optimizeTrainingPlanByDP respects time and count budgets") {
    const std::vector<training::TrainingProblemCandidate> candidates = {
        candidate(1, 1000, 20, {"动态规划"}),
        candidate(2, 1050, 20, {"双指针"}, 2),
        candidate(3, 1100, 45, {"动态规划", "双指针"}),
        candidate(4, 1150, 20, {"贪心"})
    };

    const auto selected_goal = goal();
    const auto window = training::selectStableDifficultyWindow(candidates, selected_goal);
    const auto plan = training::optimizeTrainingPlanByDP(window, selected_goal);

    REQUIRE(plan.items.size() == 2);
    REQUIRE(plan.total_estimated_minutes <= selected_goal.time_budget_minutes);
    REQUIRE(plan.covered_tags.size() == 2);
    for (const auto& item : plan.items) {
        REQUIRE(item.selected_reason.find("DP") != std::string::npos);
    }
}
