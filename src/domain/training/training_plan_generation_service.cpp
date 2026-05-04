#include "domain/training/training_plan_generation_service.hpp"

#include "algorithms/training_plan_algorithms.hpp"
#include "domain/task/task_models.hpp"
#include "domain/training/training_repository.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace training {
namespace {

std::vector<TrainingPlanItem> select_items(
    const OptimizedTrainingPlan& optimized
) {
    std::vector<TrainingPlanItem> items;
    for (const auto& selected : optimized.items) {
        TrainingPlanItem item;
        item.problem_id = selected.problem.id;
        item.order_index = static_cast<int>(items.size()) + 1;
        item.estimated_minutes = selected.problem.estimated_minutes;
        item.score = selected.score;
        item.selected_reason = selected.selected_reason;
        item.status = "not_started";
        items.push_back(std::move(item));
    }
    return items;
}

long long elapsed_microseconds(
    std::chrono::steady_clock::time_point started
) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started
    ).count();
}

nlohmann::json index_value(int index) {
    if (index < 0) {
        return nullptr;
    }
    return index + 1;
}

int weak_tag_hit_count(const StableDifficultyWindow& window) {
    int count = 0;
    for (const auto& candidate : window.candidates) {
        if (candidate.matched_tag_count > 0) {
            ++count;
        }
    }
    return count;
}

nlohmann::json candidate_window_summary(
    const StableDifficultyWindow& window,
    long long elapsed_us
) {
    return {
        {"leftIndex", index_value(window.left_index)},
        {"rightIndex", index_value(window.right_index)},
        {
            "leftDifficulty",
            window.left_index < 0
                ? nlohmann::json{nullptr}
                : nlohmann::json{window.left_difficulty}
        },
        {
            "rightDifficulty",
            window.right_index < 0
                ? nlohmann::json{nullptr}
                : nlohmann::json{window.right_difficulty}
        },
        {"difficultySpan", window.difficulty_span},
        {"candidateCount", static_cast<int>(window.candidates.size())},
        {"wrongProblemCount", window.wrong_problem_count},
        {"weakTagHitCount", weak_tag_hit_count(window)},
        {"coverageRatio", window.coverage_ratio},
        {"coveredTags", window.covered_tags},
        {"uncoveredTags", window.uncovered_tags},
        {"elapsedMicroseconds", elapsed_us}
    };
}

nlohmann::json optimization_result_summary(
    const OptimizedTrainingPlan& optimized,
    long long elapsed_us
) {
    return {
        {"selectedCount", static_cast<int>(optimized.items.size())},
        {"totalEstimatedMinutes", optimized.total_estimated_minutes},
        {"totalScore", optimized.total_score},
        {"coveredTagMask", optimized.covered_tag_mask},
        {"coveredTags", optimized.covered_tags},
        {"uncoveredTags", optimized.uncovered_tags},
        {"elapsedMicroseconds", elapsed_us}
    };
}

nlohmann::json algorithm_summary(
    const StableDifficultyWindow& window,
    const OptimizedTrainingPlan& optimized,
    long long window_elapsed_us,
    long long optimization_elapsed_us
) {
    return {
        {"algorithm", "dp"},
        {"totalElapsedMicroseconds", window_elapsed_us + optimization_elapsed_us},
        {"candidateWindow", candidate_window_summary(window, window_elapsed_us)},
        {"optimizationResult", optimization_result_summary(optimized, optimization_elapsed_us)}
    };
}

nlohmann::json brute_force_algorithm_summary(
    const BruteForceTrainingPlan& brute_force,
    long long elapsed_us
) {
    auto optimization_result = optimization_result_summary(brute_force.optimized, elapsed_us);
    optimization_result["combinationsEvaluated"] = brute_force.evaluated_combination_count;
    optimization_result["combinationLimit"] = nullptr;

    return {
        {"algorithm", "bruteforce"},
        {"totalElapsedMicroseconds", elapsed_us},
        {"candidateWindow", nullptr},
        {"optimizationResult", optimization_result}
    };
}

} // namespace

TrainingPlanGenerationService::TrainingPlanGenerationService(ConnectionPool& pool)
    : pool_(pool) {}

nlohmann::json TrainingPlanGenerationService::generatePlan(
    long long task_id,
    const GenerateTrainingPlanCommand& command
) const {
    TrainingRepository repository{pool_};
    const auto goal = repository.getGoal(command.goal_id, command.user_id);
    if (!goal) {
        throw std::runtime_error{"training goal not found"};
    }

    const auto candidates = repository.listProblemCandidates(*goal, command.include_hidden);
    OptimizedTrainingPlan optimized;
    nlohmann::json summary;

    if (command.algorithm == TrainingPlanGenerationAlgorithm::BruteForce) {
        const auto started = std::chrono::steady_clock::now();
        const auto brute_force = optimizeTrainingPlanByBruteForce(candidates, *goal);
        const auto elapsed_us = elapsed_microseconds(started);
        optimized = brute_force.optimized;
        summary = brute_force_algorithm_summary(
            brute_force,
            elapsed_us
        );
    } else {
        const auto window_started = std::chrono::steady_clock::now();
        const auto window = selectStableDifficultyWindow(candidates, *goal);
        const auto window_elapsed_us = elapsed_microseconds(window_started);

        const auto optimization_started = std::chrono::steady_clock::now();
        optimized = optimizeTrainingPlanByDP(window, *goal);
        const auto optimization_elapsed_us = elapsed_microseconds(optimization_started);
        summary = algorithm_summary(window, optimized, window_elapsed_us, optimization_elapsed_us);
    }

    auto items = select_items(optimized);

    TrainingPlan plan;
    plan.user_id = command.user_id;
    plan.goal_id = goal->id;
    plan.name = command.name.value_or(
        goal->name +
        (command.algorithm == TrainingPlanGenerationAlgorithm::BruteForce
            ? " brute force plan"
            : " plan")
    );
    plan.status = command.status.empty() ? std::string{"draft"} : command.status;
    plan.candidate_count = static_cast<int>(candidates.size());
    plan.selected_count = static_cast<int>(items.size());
    plan.total_estimated_minutes = optimized.total_estimated_minutes;
    plan.algorithm_summary = summary.dump();

    const auto plan_id = repository.createPlan(plan, items);
    const auto saved_plan = repository.getPlan(plan_id, command.user_id);
    const auto saved_items = repository.listPlanItems(plan_id);
    if (!saved_plan) {
        throw std::runtime_error{"generated training plan could not be loaded"};
    }

    return {
        {"trainingTaskId", task::task_ref_for_type(
            command.algorithm == TrainingPlanGenerationAlgorithm::BruteForce
                ? "training.plan.generate.bruteforce"
                : "training.plan.generate",
            task_id
        )},
        {"plan", *saved_plan},
        {"items", saved_items}
    };
}

} // namespace training
