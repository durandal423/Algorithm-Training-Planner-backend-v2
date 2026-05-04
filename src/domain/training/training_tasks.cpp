#include "domain/task/task_handler_registry.hpp"
#include "domain/training/training_plan_generation_service.hpp"

#include "config.hpp"

#include <optional>
#include <stdexcept>
#include <string>

namespace {

std::optional<std::string> optional_string_field(
    const nlohmann::json& payload,
    const char* key
) {
    if (!payload.contains(key) || payload.at(key).is_null()) {
        return std::nullopt;
    }
    return payload.at(key).get<std::string>();
}

training::TrainingPlanGenerationAlgorithm algorithm_for_type(const std::string& type) {
    if (type == "training.plan.generate.bruteforce") {
        return training::TrainingPlanGenerationAlgorithm::BruteForce;
    }
    return training::TrainingPlanGenerationAlgorithm::Optimized;
}

nlohmann::json run_training_plan_generation(
    task::TaskExecutionContext& context,
    const task::Task& task
) {
    if (!task.owner_user_id) {
        throw std::runtime_error{"training task is missing owner_user_id"};
    }

    const auto& payload = task.payload;
    training::GenerateTrainingPlanCommand command{
        .user_id = *task.owner_user_id,
        .goal_id = payload.at("goalId").get<long long>(),
        .include_hidden = payload.value("includeHidden", false),
        .name = optional_string_field(payload, "name"),
        .status = payload.value("status", std::string{"draft"}),
        .algorithm = algorithm_for_type(task.type)
    };

    training::TrainingPlanGenerationService generator{context.trainingPool()};
    return generator.generatePlan(task.id, command);
}

const task::TaskHandlerRegistration kOptimizedTrainingPlanTask{
    task::TaskHandlerSpec{
        .type = "training.plan.generate",
        .pool_name = "training",
        .worker_count = config::TASK_WORKER_TRAINING_THREAD_COUNT,
        .handler = run_training_plan_generation
    }
};

const task::TaskHandlerRegistration kBruteForceTrainingPlanTask{
    task::TaskHandlerSpec{
        .type = "training.plan.generate.bruteforce",
        .pool_name = "training",
        .worker_count = config::TASK_WORKER_TRAINING_THREAD_COUNT,
        .handler = run_training_plan_generation
    }
};

} // namespace
