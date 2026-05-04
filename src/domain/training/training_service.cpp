#include "domain/training/training_service.hpp"

#include "api/http_json.hpp"
#include "config.hpp"
#include "domain/training/training_plan_generation_service.hpp"
#include "domain/training/training_repository.hpp"

#include <optional>

namespace training {
namespace {

TrainingGoal goal_from_json(
    const nlohmann::json& body,
    long long user_id
) {
    TrainingGoal goal;
    goal.user_id = user_id;
    goal.name = api::required_string(body, "name");
    goal.target_count = body.value("targetCount", 10);
    goal.time_budget_minutes = body.value("timeBudgetMinutes", 180);
    goal.difficulty_min = body.value("difficultyMin", std::optional<int>{});
    goal.difficulty_max = body.value("difficultyMax", std::optional<int>{});
    goal.prefer_wrong_problems = body.value("preferWrongProblems", true);
    goal.prefer_weak_tags = body.value("preferWeakTags", true);
    goal.difficulty_weight = body.value("difficultyWeight", 10);
    goal.tag_coverage_weight = body.value("tagCoverageWeight", 20);
    goal.wrong_problem_weight = body.value("wrongProblemWeight", 15);
    goal.weak_tag_weight = body.value("weakTagWeight", 15);
    goal.estimated_time_weight = body.value("estimatedTimeWeight", 1);
    goal.tag_names = api::string_array(body, "tags");

    if (goal.name.empty()) {
        throw TrainingBadRequest{"name must not be empty"};
    }
    if (goal.target_count <= 0 ||
        goal.target_count > config::TRAINING_GOAL_MAX_TARGET_COUNT) {
        throw TrainingBadRequest{
            "targetCount must be between 1 and " +
            std::to_string(config::TRAINING_GOAL_MAX_TARGET_COUNT)
        };
    }
    if (goal.time_budget_minutes <= 0 ||
        goal.time_budget_minutes > config::TRAINING_GOAL_MAX_TIME_BUDGET_MINUTES) {
        throw TrainingBadRequest{
            "timeBudgetMinutes must be between 1 and " +
            std::to_string(config::TRAINING_GOAL_MAX_TIME_BUDGET_MINUTES)
        };
    }
    if (goal.difficulty_min && goal.difficulty_max &&
        *goal.difficulty_min > *goal.difficulty_max) {
        throw TrainingBadRequest{"difficultyMin must be less than or equal to difficultyMax"};
    }
    if (goal.tag_names.size() > config::TRAINING_GOAL_MAX_TAGS) {
        throw TrainingBadRequest{
            "tags must contain at most " +
            std::to_string(config::TRAINING_GOAL_MAX_TAGS) +
            " items"
        };
    }

    return goal;
}

const char* algorithm_name(TrainingPlanGenerationAlgorithm algorithm) {
    return algorithm == TrainingPlanGenerationAlgorithm::BruteForce
        ? "bruteforce"
        : "optimized";
}

const char* task_type(TrainingPlanGenerationAlgorithm algorithm) {
    return algorithm == TrainingPlanGenerationAlgorithm::BruteForce
        ? "training.plan.generate.bruteforce"
        : "training.plan.generate";
}

long long enqueue_plan_generation(
    ConnectionPool& pool,
    const auth::PublicUser& user,
    const nlohmann::json& body,
    const task::TaskSubmissionService& tasks,
    TrainingPlanGenerationAlgorithm algorithm
) {
    const auto goal_id = api::required_long(body, "goalId");
    const auto name = body.value("name", std::optional<std::string>{});
    const auto status = body.value("status", std::string{"draft"});

    TrainingRepository repository{pool};
    if (!repository.getGoal(goal_id, user.id)) {
        throw TrainingNotFound{"training goal not found"};
    }

    const nlohmann::json payload = {
        {"goalId", goal_id},
        {"name", name},
        {"status", status},
        {"algorithm", algorithm_name(algorithm)},
        {"includeHidden", user.role == "admin"}
    };

    return tasks.submit(
        task_type(algorithm),
        user.id,
        payload
    );
}

} // namespace

TrainingBadRequest::TrainingBadRequest(const std::string& message)
    : std::runtime_error{message} {}

TrainingNotFound::TrainingNotFound(const std::string& message)
    : std::runtime_error{message} {}

TrainingService::TrainingService(ConnectionPool& pool)
    : pool_(pool) {}

std::vector<TrainingGoal> TrainingService::listGoals(long long user_id) const {
    TrainingRepository repository{pool_};
    return repository.listGoals(user_id);
}

TrainingGoal TrainingService::createGoal(
    long long user_id,
    const nlohmann::json& body
) const {
    TrainingRepository repository{pool_};
    const auto id = repository.createGoal(goal_from_json(body, user_id));
    const auto goal = repository.getGoal(id, user_id);
    if (!goal) {
        throw TrainingNotFound{"created training goal not found"};
    }
    return *goal;
}

std::optional<TrainingGoal> TrainingService::getGoal(long long id, long long user_id) const {
    TrainingRepository repository{pool_};
    return repository.getGoal(id, user_id);
}

TrainingGoal TrainingService::updateGoal(
    long long id,
    long long user_id,
    const nlohmann::json& body
) const {
    TrainingRepository repository{pool_};
    const bool updated = repository.updateGoal(id, goal_from_json(body, user_id));
    if (!updated) {
        throw TrainingNotFound{"training goal not found"};
    }
    const auto goal = repository.getGoal(id, user_id);
    if (!goal) {
        throw TrainingNotFound{"training goal not found"};
    }
    return *goal;
}

bool TrainingService::deleteGoal(long long id, long long user_id) const {
    TrainingRepository repository{pool_};
    return repository.deleteGoal(id, user_id);
}

long long TrainingService::enqueuePlanGeneration(
    const auth::PublicUser& user,
    const nlohmann::json& body,
    const task::TaskSubmissionService& tasks
) const {
    return enqueue_plan_generation(
        pool_,
        user,
        body,
        tasks,
        TrainingPlanGenerationAlgorithm::Optimized
    );
}

long long TrainingService::enqueueBruteForcePlanGeneration(
    const auth::PublicUser& user,
    const nlohmann::json& body,
    const task::TaskSubmissionService& tasks
) const {
    return enqueue_plan_generation(
        pool_,
        user,
        body,
        tasks,
        TrainingPlanGenerationAlgorithm::BruteForce
    );
}

std::vector<TrainingPlan> TrainingService::listPlans(long long user_id) const {
    TrainingRepository repository{pool_};
    return repository.listPlans(user_id);
}

std::optional<TrainingPlanDetail> TrainingService::getPlanDetail(
    long long id,
    long long user_id
) const {
    TrainingRepository repository{pool_};
    const auto plan = repository.getPlan(id, user_id);
    if (!plan) {
        return std::nullopt;
    }
    return TrainingPlanDetail{
        .plan = *plan,
        .items = repository.listPlanItems(id)
    };
}

std::optional<std::vector<TrainingPlanItem>> TrainingService::listPlanItems(
    long long id,
    long long user_id
) const {
    TrainingRepository repository{pool_};
    const auto plan = repository.getPlan(id, user_id);
    if (!plan) {
        return std::nullopt;
    }
    return repository.listPlanItems(id);
}

} // namespace training
