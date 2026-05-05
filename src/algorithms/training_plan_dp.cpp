#include "algorithms/training_plan_algorithms.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace training {
namespace {

constexpr int kMaxMaskTags = 20;
constexpr int kNegativeInfinity = std::numeric_limits<int>::min() / 4;
constexpr std::uint64_t kMaskBits = kMaxMaskTags;
constexpr std::uint64_t kMaskValueMask = (std::uint64_t{1} << kMaxMaskTags) - 1;

std::vector<std::string> unique_goal_tags(const TrainingGoal& goal) {
    std::vector<std::string> tags;
    std::unordered_set<std::string> seen;
    for (const auto& tag : goal.tag_names) {
        if (!tag.empty() && !seen.contains(tag)) {
            tags.push_back(tag);
            seen.insert(tag);
        }
    }
    return tags;
}

std::unordered_set<std::string> tag_set(const std::vector<std::string>& tags) {
    return {tags.begin(), tags.end()};
}

std::vector<std::string> mask_tags_for_goal(const TrainingGoal& goal) {
    auto tags = unique_goal_tags(goal);
    if (static_cast<int>(tags.size()) > kMaxMaskTags) {
        tags.resize(kMaxMaskTags);
    }
    return tags;
}

std::unordered_map<std::string, int> tag_index_map(const std::vector<std::string>& tags) {
    std::unordered_map<std::string, int> index;
    for (int i = 0; i < static_cast<int>(tags.size()); ++i) {
        index[tags[i]] = i;
    }
    return index;
}

int candidate_mask(
    const TrainingProblemCandidate& candidate,
    const std::unordered_map<std::string, int>& tag_index
) {
    int mask = 0;
    for (const auto& tag : candidate.tag_names) {
        if (auto iter = tag_index.find(tag); iter != tag_index.end()) {
            mask |= (1 << iter->second);
        }
    }
    return mask;
}

int bit_count(int mask) {
    return std::popcount(static_cast<unsigned>(mask));
}

std::vector<std::string> tags_for_mask(int mask, const std::vector<std::string>& tags) {
    std::vector<std::string> covered;
    for (int i = 0; i < static_cast<int>(tags.size()); ++i) {
        if ((mask & (1 << i)) != 0) {
            covered.push_back(tags[i]);
        }
    }
    return covered;
}

std::vector<std::string> uncovered_tags(
    const std::vector<std::string>& requested,
    const std::vector<std::string>& covered
) {
    const auto covered_set = tag_set(covered);
    std::vector<std::string> uncovered;
    for (const auto& tag : requested) {
        if (!covered_set.contains(tag)) {
            uncovered.push_back(tag);
        }
    }
    return uncovered;
}

std::string join_tags(const std::vector<std::string>& tags) {
    std::string text;
    for (std::size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) {
            text += ",";
        }
        text += tags[i];
    }
    return text;
}

int score_problem_for_training_goal(
    const TrainingProblemCandidate& candidate,
    const TrainingGoal& goal,
    int window_center_difficulty,
    const std::unordered_set<std::string>& requested
) {
    int target_hits = 0;
    for (const auto& tag : candidate.tag_names) {
        if (requested.contains(tag)) {
            ++target_hits;
        }
    }

    int score = 50;
    score += target_hits * goal.tag_coverage_weight;
    if (goal.prefer_wrong_problems && candidate.wrong_attempt_count > 0) {
        score += goal.wrong_problem_weight + candidate.wrong_attempt_count * 3;
    }

    const int distance = std::abs(candidate.difficulty - window_center_difficulty);
    score += std::max(0, 1000 - distance) / 100 * std::max(1, goal.difficulty_weight);
    score -= candidate.estimated_minutes * std::max(1, goal.estimated_time_weight);
    return std::max(1, score);
}

std::vector<std::string> covered_target_tags(
    const TrainingProblemCandidate& candidate,
    const std::unordered_set<std::string>& requested
) {
    std::vector<std::string> covered;
    for (const auto& tag : candidate.tag_names) {
        if (requested.contains(tag)) {
            covered.push_back(tag);
        }
    }
    std::sort(covered.begin(), covered.end());
    covered.erase(std::unique(covered.begin(), covered.end()), covered.end());
    return covered;
}

std::string selection_reason(
    const TrainingProblemCandidate& candidate,
    const std::vector<std::string>& target_tags,
    int score
) {
    std::string reason = "双指针稳定窗口候选；DP最优转移选中";
    if (!target_tags.empty()) {
        reason += "；覆盖目标标签=" + join_tags(target_tags);
    }
    if (candidate.wrong_attempt_count > 0) {
        reason += "；历史非AC次数=" + std::to_string(candidate.wrong_attempt_count);
    }
    reason += "；score=" + std::to_string(score);
    return reason;
}

struct State {
    int score{kNegativeInfinity};
    int node_id{-1};
};

struct SelectionNode {
    int previous_node_id{-1};
    int candidate_index{-1};
};

std::uint64_t pack_key(int minutes, int mask) {
    return (static_cast<std::uint64_t>(minutes) << kMaskBits) |
           static_cast<std::uint64_t>(mask);
}

int key_minutes(std::uint64_t key) {
    return static_cast<int>(key >> kMaskBits);
}

int key_mask(std::uint64_t key) {
    return static_cast<int>(key & kMaskValueMask);
}

bool better_state(
    const State& candidate,
    int candidate_count,
    int candidate_mask_value,
    int candidate_minutes,
    const State& best,
    int best_count,
    int best_mask_value,
    int best_minutes
) {
    if (best.score == kNegativeInfinity) {
        return true;
    }
    if (candidate_count != best_count) {
        return candidate_count > best_count;
    }
    if (candidate.score != best.score) {
        return candidate.score > best.score;
    }
    if (bit_count(candidate_mask_value) != bit_count(best_mask_value)) {
        return bit_count(candidate_mask_value) > bit_count(best_mask_value);
    }
    return candidate_minutes < best_minutes;
}

OptimizedTrainingPlan optimize_candidates_by_sparse_dp(
    const std::vector<TrainingProblemCandidate>& candidates,
    const TrainingGoal& goal,
    const std::vector<std::string>& mask_tags,
    int center_difficulty
) {
    OptimizedTrainingPlan result;
    const auto requested_tags = unique_goal_tags(goal);
    if (candidates.empty() || goal.target_count <= 0 || goal.time_budget_minutes <= 0) {
        result.uncovered_tags = requested_tags;
        return result;
    }

    const int max_count = std::min<int>(goal.target_count, candidates.size());
    const int budget = goal.time_budget_minutes;
    const auto tag_index = tag_index_map(mask_tags);
    const auto requested = tag_set(requested_tags);

    std::vector<std::unordered_map<std::uint64_t, State>> dp(max_count + 1);
    std::vector<SelectionNode> nodes;
    dp[0][pack_key(0, 0)] = State{.score = 0, .node_id = -1};

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        const auto& candidate = candidates[i];
        const int minutes = candidate.estimated_minutes;
        if (minutes <= 0 || minutes > budget) {
            continue;
        }

        const int item_score = score_problem_for_training_goal(
            candidate,
            goal,
            center_difficulty,
            requested
        );
        const int item_mask = candidate_mask(candidate, tag_index);

        for (int count = max_count - 1; count >= 0; --count) {
            if (dp[count].empty()) {
                continue;
            }

            for (const auto& [key, previous] : dp[count]) {
                const int used_minutes = key_minutes(key);
                const int next_minutes = used_minutes + minutes;
                if (next_minutes > budget || previous.score == kNegativeInfinity) {
                    continue;
                }

                const int next_mask = key_mask(key) | item_mask;
                const int next_score = previous.score + item_score;
                const auto next_key = pack_key(next_minutes, next_mask);
                auto& destination = dp[count + 1];
                const auto existing = destination.find(next_key);
                if (existing != destination.end() && existing->second.score >= next_score) {
                    continue;
                }

                const int node_id = static_cast<int>(nodes.size());
                nodes.push_back(SelectionNode{
                    .previous_node_id = previous.node_id,
                    .candidate_index = i
                });
                destination[next_key] = State{
                    .score = next_score,
                    .node_id = node_id
                };
            }
        }
    }

    State best;
    int best_minutes = 0;
    int best_count = 0;
    int best_mask = 0;
    for (int count = 0; count <= max_count; ++count) {
        for (const auto& [key, state] : dp[count]) {
            const int minutes = key_minutes(key);
            const int mask = key_mask(key);
            if (better_state(state, count, mask, minutes, best, best_count, best_mask, best_minutes)) {
                best = state;
                best_minutes = minutes;
                best_count = count;
                best_mask = mask;
            }
        }
    }

    result.total_score = std::max(0, best.score);
    result.total_estimated_minutes = best_minutes;
    result.covered_tag_mask = best_mask;
    result.covered_tags = tags_for_mask(best_mask, mask_tags);
    result.uncovered_tags = uncovered_tags(requested_tags, result.covered_tags);

    std::vector<int> selected_indices;
    for (int node_id = best.node_id; node_id >= 0;) {
        const auto& node = nodes[node_id];
        selected_indices.push_back(node.candidate_index);
        node_id = node.previous_node_id;
    }
    std::reverse(selected_indices.begin(), selected_indices.end());

    for (const int index : selected_indices) {
        const auto& candidate = candidates[index];
        const auto target_tags = covered_target_tags(candidate, requested);
        const int score = score_problem_for_training_goal(candidate, goal, center_difficulty, requested);
        result.items.push_back(SelectedTrainingProblem{
            .problem = candidate,
            .score = score,
            .covered_target_tags = target_tags,
            .selected_reason = selection_reason(candidate, target_tags, score)
        });
    }

    std::stable_sort(result.items.begin(), result.items.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.problem.difficulty != rhs.problem.difficulty) {
            return lhs.problem.difficulty < rhs.problem.difficulty;
        }
        return lhs.problem.id < rhs.problem.id;
    });
    return result;
}

} // namespace

int scoreProblemForTrainingGoal(
    const TrainingProblemCandidate& candidate,
    const TrainingGoal& goal,
    int window_center_difficulty
) {
    const auto requested = tag_set(unique_goal_tags(goal));
    return score_problem_for_training_goal(candidate, goal, window_center_difficulty, requested);
}

OptimizedTrainingPlan optimizeTrainingPlanByDP(
    const StableDifficultyWindow& window,
    const TrainingGoal& goal
) {
    if (window.candidates.empty() || goal.target_count <= 0 || goal.time_budget_minutes <= 0) {
        OptimizedTrainingPlan result;
        result.uncovered_tags = unique_goal_tags(goal);
        return result;
    }

    const auto mask_tags = mask_tags_for_goal(goal);
    const int center_difficulty = (window.left_difficulty + window.right_difficulty) / 2;
    return optimize_candidates_by_sparse_dp(
        window.candidates,
        goal,
        mask_tags,
        center_difficulty
    );
}

} // namespace training
