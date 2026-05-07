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

std::vector<std::string> all_tags() {
    return {
        "双指针",
        "滑动窗口",
        "动态规划",
        "贪心",
        "图论",
        "搜索",
        "数论",
        "数据结构",
        "状态压缩",
        "排序",
        "哈希表",
        "连通性",
        "前缀和",
        "二分"
    };
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

TEST_CASE("selectStableDifficultyWindow fallback stays bounded when tags are spread out") {
    std::vector<training::TrainingProblemCandidate> candidates;
    for (int i = 0; i < 10; ++i) {
        candidates.push_back(candidate(i + 1, 1000 + i, 10, {"动态规划"}));
    }
    for (int i = 0; i < 10; ++i) {
        candidates.push_back(candidate(100 + i, 2000 + i, 10, {"双指针"}));
    }
    for (int i = 0; i < 10; ++i) {
        candidates.push_back(candidate(200 + i, 3000 + i, 10, {"图论"}));
    }

    auto selected_goal = goal();
    selected_goal.target_count = 1;
    selected_goal.tag_names = {"动态规划", "双指针", "图论"};

    const auto window = training::selectStableDifficultyWindow(candidates, selected_goal);

    REQUIRE(window.candidates.size() == 3);
    REQUIRE(window.covered_tags.size() == 2);
    REQUIRE(window.uncovered_tags.size() == 1);
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

TEST_CASE("optimizeTrainingPlanByDP prefers requested count before raw score") {
    std::vector<training::TrainingProblemCandidate> candidates;
    for (int i = 0; i < 6; ++i) {
        candidates.push_back(candidate(i + 1, 1500 + i, 30, {"动态规划"}, 100));
    }
    for (int i = 0; i < 10; ++i) {
        candidates.push_back(candidate(100 + i, 2600 + i, 18, {"动态规划"}));
    }

    auto selected_goal = goal();
    selected_goal.target_count = 10;
    selected_goal.time_budget_minutes = 180;
    selected_goal.tag_names = {"动态规划"};

    training::StableDifficultyWindow window;
    window.left_index = 0;
    window.right_index = static_cast<int>(candidates.size()) - 1;
    window.left_difficulty = 1500;
    window.right_difficulty = 2609;
    window.difficulty_span = 1109;
    window.covered_tags = selected_goal.tag_names;
    window.candidates = std::move(candidates);

    const auto plan = training::optimizeTrainingPlanByDP(window, selected_goal);

    REQUIRE(plan.items.size() == 10);
    REQUIRE(plan.total_estimated_minutes <= selected_goal.time_budget_minutes);
}

TEST_CASE("optimizeTrainingPlanByBruteForce enumerates feasible combinations") {
    const std::vector<training::TrainingProblemCandidate> candidates = {
        candidate(1, 1000, 20, {"动态规划"}),
        candidate(2, 1050, 20, {"双指针"}, 1),
        candidate(3, 1100, 45, {"动态规划", "双指针"}),
        candidate(4, 1150, 20, {"贪心"})
    };

    const auto selected_goal = goal();
    const auto brute_force = training::optimizeTrainingPlanByBruteForce(candidates, selected_goal);

    REQUIRE(brute_force.considered_candidate_count == 3);
    REQUIRE(brute_force.evaluated_combination_count == 6);
    REQUIRE(brute_force.optimized.items.size() == 2);
    REQUIRE(brute_force.optimized.total_estimated_minutes <= selected_goal.time_budget_minutes);
    REQUIRE(brute_force.optimized.covered_tags.size() == 2);
    for (const auto& item : brute_force.optimized.items) {
        REQUIRE(item.selected_reason.find("暴力枚举") != std::string::npos);
    }
}

TEST_CASE("optimizeTrainingPlanByBruteForce uses a filtered difficulty window") {
    std::vector<training::TrainingProblemCandidate> candidates;
    for (int i = 0; i < 24; ++i) {
        candidates.push_back(candidate(
            i + 1,
            1000 + i,
            10,
            {i % 2 == 0 ? "动态规划" : "双指针"}
        ));
    }
    candidates.push_back(candidate(101, 1300, 40, {"动态规划", "双指针"}, 100));
    candidates.push_back(candidate(102, 1301, 40, {"动态规划", "双指针"}, 100));

    auto selected_goal = goal();
    selected_goal.time_budget_minutes = 90;

    const auto brute_force = training::optimizeTrainingPlanByBruteForce(candidates, selected_goal);

    REQUIRE(brute_force.considered_candidate_count < static_cast<int>(candidates.size()));
    REQUIRE(brute_force.optimized.items.size() == 2);

    bool selected_first_late_candidate = false;
    bool selected_second_late_candidate = false;
    for (const auto& item : brute_force.optimized.items) {
        selected_first_late_candidate |= item.problem.id == 101;
        selected_second_late_candidate |= item.problem.id == 102;
    }
    REQUIRE_FALSE(selected_first_late_candidate);
    REQUIRE_FALSE(selected_second_late_candidate);
}

TEST_CASE("optimizeTrainingPlanByBruteForce has no hidden target cap") {
    std::vector<training::TrainingProblemCandidate> candidates;
    for (int i = 0; i < 13; ++i) {
        candidates.push_back(candidate(i + 1, 1000 + i, 10, {"动态规划"}));
    }

    auto selected_goal = goal();
    selected_goal.target_count = 13;
    selected_goal.time_budget_minutes = 180;
    selected_goal.tag_names = {"动态规划"};

    const auto brute_force = training::optimizeTrainingPlanByBruteForce(candidates, selected_goal);

    REQUIRE(brute_force.considered_candidate_count == 13);
    REQUIRE(brute_force.evaluated_combination_count == 8191);
    REQUIRE(brute_force.optimized.items.size() == 13);
}

TEST_CASE("training plan algorithms support all tags and target count twelve") {
    const auto tags = all_tags();
    std::vector<training::TrainingProblemCandidate> candidates;
    for (int i = 0; i < 12; ++i) {
        candidates.push_back(candidate(
            i + 1,
            800 + i * 2,
            15 + (i % 4),
            {
                tags[i % tags.size()],
                tags[(i + 5) % tags.size()],
                tags[(i + 9) % tags.size()]
            },
            i % 11 == 0 ? 1 : 0
        ));
    }

    auto selected_goal = goal();
    selected_goal.target_count = 12;
    selected_goal.time_budget_minutes = 240;
    selected_goal.tag_names = tags;

    const auto window = training::selectStableDifficultyWindow(candidates, selected_goal);
    const auto dp_plan = training::optimizeTrainingPlanByDP(window, selected_goal);
    const auto brute_force = training::optimizeTrainingPlanByBruteForce(candidates, selected_goal);

    REQUIRE(dp_plan.items.size() == 12);
    REQUIRE(dp_plan.total_estimated_minutes <= selected_goal.time_budget_minutes);
    REQUIRE(dp_plan.covered_tags.size() == tags.size());
    REQUIRE(brute_force.optimized.items.size() == 12);
    REQUIRE(brute_force.optimized.total_estimated_minutes <= selected_goal.time_budget_minutes);
    REQUIRE(brute_force.optimized.covered_tags.size() == tags.size());
    REQUIRE(brute_force.evaluated_combination_count > 0);
}

TEST_CASE("TrainingPlan serializes JSON algorithm summaries as structured data") {
    training::TrainingPlan plan;
    plan.id = 12;
    plan.user_id = 1;
    plan.name = "第 1 周训练";
    plan.status = "draft";
    plan.candidate_count = 20;
    plan.selected_count = 8;
    plan.total_estimated_minutes = 175;
    plan.algorithm_summary = R"JSON({
        "algorithm": "dp",
        "totalElapsedMicroseconds": 18432,
        "optimizationResult": {
            "selectedCount": 8,
            "totalEstimatedMinutes": 175,
            "totalScore": 574,
            "elapsedMicroseconds": 15222
        }
    })JSON";

    const nlohmann::json json = plan;

    REQUIRE(json.at("algorithmSummary").is_object());
    REQUIRE(json.at("algorithmSummary").at("algorithm") == "dp");
    REQUIRE(json.at("algorithmSummary").at("totalElapsedMicroseconds") == 18432);
    REQUIRE(json.at("algorithmSummary").at("optimizationResult").at("selectedCount") == 8);
}

TEST_CASE("TrainingPlanItem serializes selected problem tags") {
    training::TrainingPlanItem item;
    item.id = 1;
    item.plan_id = 12;
    item.problem_id = 34;
    item.order_index = 1;
    item.estimated_minutes = 25;
    item.score = 145;
    item.selected_reason = "覆盖目标标签=双指针,滑动窗口";
    item.tag_names = {"双指针", "滑动窗口"};

    const nlohmann::json json = item;

    REQUIRE(json.at("tags").is_array());
    REQUIRE(json.at("tags").size() == 2);
    REQUIRE(json.at("tags").at(0) == "双指针");
    REQUIRE(json.at("tags").at(1) == "滑动窗口");
}
