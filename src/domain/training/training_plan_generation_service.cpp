#include "domain/training/training_plan_generation_service.hpp"

#include "algorithms/training_plan_algorithms.hpp"
#include "domain/training/training_repository.hpp"

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
    const OptimizedTrainingPlan& optimized
) {
    return "selectStableDifficultyWindow: two-pointer stable difficulty window [" +
        std::to_string(window.left_difficulty) + ", " +
        std::to_string(window.right_difficulty) + "], span=" +
        std::to_string(window.difficulty_span) + ", windowCandidates=" +
        std::to_string(window.candidates.size()) +
        "; optimizeTrainingPlanByDP: 0/1 knapsack by time budget and target count, totalScore=" +
        std::to_string(optimized.total_score);
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
    const auto window = selectStableDifficultyWindow(candidates, *goal);
    const auto optimized = optimizeTrainingPlanByDP(window, *goal);
    auto items = select_items(optimized);

    TrainingPlan plan;
    plan.user_id = command.user_id;
    plan.goal_id = goal->id;
    plan.name = command.name.value_or(goal->name + " plan");
    plan.status = command.status.empty() ? std::string{"draft"} : command.status;
    plan.candidate_count = static_cast<int>(candidates.size());
    plan.selected_count = static_cast<int>(items.size());
    plan.total_estimated_minutes = optimized.total_estimated_minutes;
    plan.algorithm_summary = algorithm_summary(window, optimized);

    const auto plan_id = repository.createPlan(plan, items);
    const auto saved_plan = repository.getPlan(plan_id, command.user_id);
    const auto saved_items = repository.listPlanItems(plan_id);
    if (!saved_plan) {
        throw std::runtime_error{"generated training plan could not be loaded"};
    }

    return {
        {"taskId", task_id},
        {"plan", *saved_plan},
        {"items", saved_items}
    };
}

} // namespace training
