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

std::string algorithm_summary(
    const StableDifficultyWindow& window,
    const OptimizedTrainingPlan& optimized,
    long long elapsed_ms
) {
    return "selectStableDifficultyWindow: two-pointer stable difficulty window [" +
        std::to_string(window.left_difficulty) + ", " +
        std::to_string(window.right_difficulty) + "], span=" +
        std::to_string(window.difficulty_span) + ", windowCandidates=" +
        std::to_string(window.candidates.size()) +
        "; optimizeTrainingPlanByDP: 0/1 knapsack by time budget and target count, totalScore=" +
        std::to_string(optimized.total_score) + ", elapsedMs=" +
        std::to_string(elapsed_ms);
}

std::string brute_force_algorithm_summary(
    const BruteForceTrainingPlan& brute_force,
    int candidate_count,
    long long elapsed_ms
) {
    return "optimizeTrainingPlanByBruteForce: exhaustive DFS over candidate combinations, candidateCount=" +
        std::to_string(candidate_count) + ", consideredCandidates=" +
        std::to_string(brute_force.considered_candidate_count) + ", evaluatedCombinations=" +
        std::to_string(brute_force.evaluated_combination_count) + ", totalScore=" +
        std::to_string(brute_force.optimized.total_score) + ", elapsedMs=" +
        std::to_string(elapsed_ms);
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
    std::string summary;

    const auto started = std::chrono::steady_clock::now();
    if (command.algorithm == TrainingPlanGenerationAlgorithm::BruteForce) {
        const auto brute_force = optimizeTrainingPlanByBruteForce(candidates, *goal);
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started
        ).count();
        optimized = brute_force.optimized;
        summary = brute_force_algorithm_summary(
            brute_force,
            static_cast<int>(candidates.size()),
            elapsed_ms
        );
    } else {
        const auto window = selectStableDifficultyWindow(candidates, *goal);
        optimized = optimizeTrainingPlanByDP(window, *goal);
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started
        ).count();
        summary = algorithm_summary(window, optimized, elapsed_ms);
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
    plan.algorithm_summary = summary;

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
