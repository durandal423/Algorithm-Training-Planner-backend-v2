#include "domain/training/training_service.hpp"

#include "config.hpp"
#include "domain/training/training_repository.hpp"

#include <algorithm>
#include <optional>

namespace training {
namespace {

std::optional<std::string> optional_string(const nlohmann::json& body, const char* key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        return std::nullopt;
    }
    return body.at(key).get<std::string>();
}

long long required_long(const nlohmann::json& body, const char* key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        throw TrainingBadRequest{std::string{key} + " is required"};
    }
    return body.at(key).get<long long>();
}

std::vector<std::string> string_array(const nlohmann::json& body, const char* key) {
    std::vector<std::string> values;
    if (!body.contains(key) || !body.at(key).is_array()) {
        return values;
    }
    for (const auto& value : body.at(key)) {
        if (value.is_string()) {
            values.push_back(value.get<std::string>());
        }
    }
    return values;
}

TrainingGoal goal_from_json(
    const nlohmann::json& body,
    long long user_id
) {
    if (!body.contains("name") || !body.at("name").is_string()) {
        throw TrainingBadRequest{"name is required"};
    }

    TrainingGoal goal;
    goal.user_id = user_id;
    goal.name = body.at("name").get<std::string>();
    goal.target_count = body.value("targetCount", 10);
    goal.time_budget_minutes = body.value("timeBudgetMinutes", 180);
    goal.difficulty_min = body.contains("difficultyMin") && !body.at("difficultyMin").is_null()
        ? std::optional<int>{body.at("difficultyMin").get<int>()}
        : std::nullopt;
    goal.difficulty_max = body.contains("difficultyMax") && !body.at("difficultyMax").is_null()
        ? std::optional<int>{body.at("difficultyMax").get<int>()}
        : std::nullopt;
    goal.prefer_wrong_problems = body.value("preferWrongProblems", true);
    goal.prefer_weak_tags = body.value("preferWeakTags", true);
    goal.difficulty_weight = body.value("difficultyWeight", 10);
    goal.tag_coverage_weight = body.value("tagCoverageWeight", 20);
    goal.wrong_problem_weight = body.value("wrongProblemWeight", 15);
    goal.weak_tag_weight = body.value("weakTagWeight", 15);
    goal.estimated_time_weight = body.value("estimatedTimeWeight", 1);
    goal.tag_names = string_array(body, "tags");

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
    const task::TaskScheduler& scheduler,
    const TrainingPlanGenerationService& generator
) const {
    const auto goal_id = required_long(body, "goalId");
    const auto name = optional_string(body, "name");
    const auto status = body.value("status", std::string{"draft"});

    TrainingRepository repository{pool_};
    if (!repository.getGoal(goal_id, user.id)) {
        throw TrainingNotFound{"training goal not found"};
    }

    GenerateTrainingPlanCommand command{
        .user_id = user.id,
        .goal_id = goal_id,
        .include_hidden = user.role == "admin",
        .name = name,
        .status = status
    };

    const nlohmann::json payload = {
        {"goalId", goal_id},
        {"name", name},
        {"status", status}
    };

    return scheduler.submit(
        "training.plan.generate",
        user.id,
        payload,
        [&generator, command](long long task_id) {
            return generator.generatePlan(task_id, command);
        }
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
