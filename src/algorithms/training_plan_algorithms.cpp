#include "algorithms/training_plan_algorithms.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace training {
namespace {

constexpr int kMaxMaskTags = 20;
constexpr int kNegativeInfinity = std::numeric_limits<int>::min() / 4;

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

std::vector<TrainingProblemCandidate> sorted_candidates(
    const std::vector<TrainingProblemCandidate>& candidates
) {
    auto sorted = candidates;
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.difficulty != rhs.difficulty) {
            return lhs.difficulty < rhs.difficulty;
        }
        if (lhs.estimated_minutes != rhs.estimated_minutes) {
            return lhs.estimated_minutes < rhs.estimated_minutes;
        }
        return lhs.id < rhs.id;
    });
    return sorted;
}

std::unordered_set<std::string> available_goal_tags(
    const std::vector<TrainingProblemCandidate>& candidates,
    const std::vector<std::string>& requested
) {
    const auto requested_set = tag_set(requested);
    std::unordered_set<std::string> available;
    for (const auto& candidate : candidates) {
        for (const auto& tag : candidate.tag_names) {
            if (requested_set.contains(tag)) {
                available.insert(tag);
            }
        }
    }
    return available;
}

int desired_window_size(int candidate_count, int target_count) {
    if (candidate_count <= 0 || target_count <= 0) {
        return 0;
    }
    return std::min(candidate_count, std::max(target_count, target_count * 3));
}

struct WindowStats {
    int wrong_count{};
    std::unordered_map<std::string, int> tag_counts;
};

void add_candidate(WindowStats& stats, const TrainingProblemCandidate& candidate) {
    if (candidate.wrong_attempt_count > 0) {
        ++stats.wrong_count;
    }
    for (const auto& tag : candidate.tag_names) {
        ++stats.tag_counts[tag];
    }
}

void remove_candidate(WindowStats& stats, const TrainingProblemCandidate& candidate) {
    if (candidate.wrong_attempt_count > 0) {
        --stats.wrong_count;
    }
    for (const auto& tag : candidate.tag_names) {
        auto iter = stats.tag_counts.find(tag);
        if (iter == stats.tag_counts.end()) {
            continue;
        }
        --iter->second;
        if (iter->second <= 0) {
            stats.tag_counts.erase(iter);
        }
    }
}

std::vector<std::string> covered_tags(
    const WindowStats& stats,
    const std::vector<std::string>& requested
) {
    std::vector<std::string> covered;
    for (const auto& tag : requested) {
        if (stats.tag_counts.contains(tag)) {
            covered.push_back(tag);
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

int available_coverage_count(
    const WindowStats& stats,
    const std::unordered_set<std::string>& available
) {
    int count = 0;
    for (const auto& tag : available) {
        if (stats.tag_counts.contains(tag)) {
            ++count;
        }
    }
    return count;
}

bool is_valid_window(
    const WindowStats& stats,
    int size,
    int min_size,
    const std::unordered_set<std::string>& available
) {
    return size >= min_size &&
           available_coverage_count(stats, available) == static_cast<int>(available.size());
}

StableDifficultyWindow make_window(
    const std::vector<TrainingProblemCandidate>& sorted,
    int left,
    int right,
    const WindowStats& stats,
    const std::vector<std::string>& requested,
    int available_count
) {
    StableDifficultyWindow window;
    window.left_index = left;
    window.right_index = right;
    window.left_difficulty = sorted[left].difficulty;
    window.right_difficulty = sorted[right].difficulty;
    window.difficulty_span = window.right_difficulty - window.left_difficulty;
    window.wrong_problem_count = stats.wrong_count;
    window.covered_tags = covered_tags(stats, requested);
    window.uncovered_tags = uncovered_tags(requested, window.covered_tags);
    window.coverage_ratio = available_count == 0
        ? 1.0
        : static_cast<double>(window.covered_tags.size()) / static_cast<double>(available_count);
    window.candidates.assign(sorted.begin() + left, sorted.begin() + right + 1);
    return window;
}

bool better_window(
    const StableDifficultyWindow& candidate,
    const StableDifficultyWindow& best,
    const TrainingGoal& goal
) {
    if (best.left_index < 0) {
        return true;
    }
    if (candidate.difficulty_span != best.difficulty_span) {
        return candidate.difficulty_span < best.difficulty_span;
    }
    if (candidate.covered_tags.size() != best.covered_tags.size()) {
        return candidate.covered_tags.size() > best.covered_tags.size();
    }
    if (goal.prefer_wrong_problems &&
        candidate.wrong_problem_count != best.wrong_problem_count) {
        return candidate.wrong_problem_count > best.wrong_problem_count;
    }
    return candidate.candidates.size() < best.candidates.size();
}

std::vector<std::string> mask_tags_for_goal(
    const StableDifficultyWindow& window,
    const TrainingGoal& goal
) {
    std::vector<std::string> tags;
    std::unordered_set<std::string> seen;
    for (const auto& tag : unique_goal_tags(goal)) {
        if (!seen.contains(tag)) {
            tags.push_back(tag);
            seen.insert(tag);
            if (static_cast<int>(tags.size()) == kMaxMaskTags) {
                return tags;
            }
        }
    }
    for (const auto& candidate : window.candidates) {
        for (const auto& tag : candidate.tag_names) {
            if (!seen.contains(tag)) {
                tags.push_back(tag);
                seen.insert(tag);
                if (static_cast<int>(tags.size()) == kMaxMaskTags) {
                    return tags;
                }
            }
        }
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

struct State {
    int score{kNegativeInfinity};
    int node_id{-1};
};

struct SelectionNode {
    int previous_node_id{-1};
    int candidate_index{-1};
};

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

std::string brute_force_selection_reason(
    const TrainingProblemCandidate& candidate,
    const std::vector<std::string>& target_tags,
    int score
) {
    std::string reason = "暴力枚举对照组；无组合上限状态搜索选中";
    if (!target_tags.empty()) {
        reason += "；覆盖目标标签=" + join_tags(target_tags);
    }
    if (candidate.wrong_attempt_count > 0) {
        reason += "；历史非AC次数=" + std::to_string(candidate.wrong_attempt_count);
    }
    reason += "；score=" + std::to_string(score);
    return reason;
}

using SelectionReasonBuilder = std::string (*)(
    const TrainingProblemCandidate&,
    const std::vector<std::string>&,
    int
);

OptimizedTrainingPlan optimize_candidates_by_sparse_dp(
    const std::vector<TrainingProblemCandidate>& candidates,
    const TrainingGoal& goal,
    const std::vector<std::string>& mask_tags,
    int center_difficulty,
    SelectionReasonBuilder reason_builder,
    long long* evaluated_transition_count = nullptr
) {
    OptimizedTrainingPlan result;
    if (candidates.empty() || goal.target_count <= 0 || goal.time_budget_minutes <= 0) {
        result.uncovered_tags = unique_goal_tags(goal);
        return result;
    }

    const int max_count = std::min<int>(goal.target_count, candidates.size());
    const int budget = goal.time_budget_minutes;
    const auto tag_index = tag_index_map(mask_tags);

    std::vector<std::vector<std::unordered_map<int, State>>> dp(
        max_count + 1,
        std::vector<std::unordered_map<int, State>>(budget + 1)
    );
    std::vector<SelectionNode> nodes;
    dp[0][0][0] = State{.score = 0, .node_id = -1};

    for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
        const auto& candidate = candidates[i];
        const int minutes = candidate.estimated_minutes;
        if (minutes <= 0 || minutes > budget) {
            continue;
        }

        const int item_score = scoreProblemForTrainingGoal(candidate, goal, center_difficulty);
        const int item_mask = candidate_mask(candidate, tag_index);

        for (int count = max_count - 1; count >= 0; --count) {
            for (int used_minutes = budget - minutes; used_minutes >= 0; --used_minutes) {
                const auto& source = dp[count][used_minutes];
                if (source.empty()) {
                    continue;
                }

                for (const auto& [mask, previous] : source) {
                    if (previous.score == kNegativeInfinity) {
                        continue;
                    }

                    const int next_minutes = used_minutes + minutes;
                    const int next_count = count + 1;
                    const int next_mask = mask | item_mask;
                    const int next_score = previous.score + item_score;
                    auto& destination = dp[next_count][next_minutes];
                    const auto existing = destination.find(next_mask);
                    if (existing != destination.end() && existing->second.score >= next_score) {
                        continue;
                    }

                    if (evaluated_transition_count != nullptr) {
                        ++(*evaluated_transition_count);
                    }
                    const int node_id = static_cast<int>(nodes.size());
                    nodes.push_back(SelectionNode{
                        .previous_node_id = previous.node_id,
                        .candidate_index = i
                    });
                    destination[next_mask] = State{
                        .score = next_score,
                        .node_id = node_id
                    };
                }
            }
        }
    }

    State best;
    int best_minutes = 0;
    int best_count = 0;
    int best_mask = 0;
    for (int count = 0; count <= max_count; ++count) {
        for (int minutes = 0; minutes <= budget; ++minutes) {
            for (const auto& [mask, state] : dp[count][minutes]) {
                if (state.score == kNegativeInfinity) {
                    continue;
                }
                if (better_state(state, count, mask, minutes, best, best_count, best_mask, best_minutes)) {
                    best = state;
                    best_minutes = minutes;
                    best_count = count;
                    best_mask = mask;
                }
            }
        }
    }

    result.total_score = std::max(0, best.score);
    result.total_estimated_minutes = best_minutes;
    result.covered_tag_mask = best_mask;
    result.covered_tags = tags_for_mask(best_mask, mask_tags);
    result.uncovered_tags = uncovered_tags(unique_goal_tags(goal), result.covered_tags);

    std::vector<int> selected_indices;
    for (int node_id = best.node_id; node_id >= 0;) {
        const auto& node = nodes[node_id];
        selected_indices.push_back(node.candidate_index);
        node_id = node.previous_node_id;
    }
    std::reverse(selected_indices.begin(), selected_indices.end());

    const auto requested = tag_set(unique_goal_tags(goal));
    for (const int index : selected_indices) {
        const auto& candidate = candidates[index];
        const auto target_tags = covered_target_tags(candidate, requested);
        const int score = scoreProblemForTrainingGoal(candidate, goal, center_difficulty);
        result.items.push_back(SelectedTrainingProblem{
            .problem = candidate,
            .score = score,
            .covered_target_tags = target_tags,
            .selected_reason = reason_builder(candidate, target_tags, score)
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

StableDifficultyWindow selectStableDifficultyWindow(
    const std::vector<TrainingProblemCandidate>& candidates,
    const TrainingGoal& goal
) {
    const auto sorted = sorted_candidates(candidates);
    const auto requested = unique_goal_tags(goal);
    if (sorted.empty() || goal.target_count <= 0) {
        return {};
    }

    const int min_size = desired_window_size(static_cast<int>(sorted.size()), goal.target_count);
    const auto available = available_goal_tags(sorted, requested);
    StableDifficultyWindow best;
    WindowStats stats;
    int left = 0;

    for (int right = 0; right < static_cast<int>(sorted.size()); ++right) {
        add_candidate(stats, sorted[right]);

        while (left <= right &&
               is_valid_window(stats, right - left + 1, min_size, available)) {
            auto candidate = make_window(
                sorted,
                left,
                right,
                stats,
                requested,
                static_cast<int>(available.size())
            );
            if (better_window(candidate, best, goal)) {
                best = std::move(candidate);
            }

            remove_candidate(stats, sorted[left]);
            ++left;
        }
    }

    if (best.left_index >= 0) {
        return best;
    }

    WindowStats fallback_stats;
    for (const auto& candidate : sorted) {
        add_candidate(fallback_stats, candidate);
    }
    return make_window(
        sorted,
        0,
        static_cast<int>(sorted.size()) - 1,
        fallback_stats,
        requested,
        static_cast<int>(available.size())
    );
}

int scoreProblemForTrainingGoal(
    const TrainingProblemCandidate& candidate,
    const TrainingGoal& goal,
    int window_center_difficulty
) {
    const auto requested = tag_set(unique_goal_tags(goal));
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

OptimizedTrainingPlan optimizeTrainingPlanByDP(
    const StableDifficultyWindow& window,
    const TrainingGoal& goal
) {
    if (window.candidates.empty() || goal.target_count <= 0 || goal.time_budget_minutes <= 0) {
        OptimizedTrainingPlan result;
        result.uncovered_tags = unique_goal_tags(goal);
        return result;
    }

    const auto mask_tags = mask_tags_for_goal(window, goal);
    const int center_difficulty = (window.left_difficulty + window.right_difficulty) / 2;
    return optimize_candidates_by_sparse_dp(
        window.candidates,
        goal,
        mask_tags,
        center_difficulty,
        selection_reason
    );
}

BruteForceTrainingPlan optimizeTrainingPlanByBruteForce(
    const std::vector<TrainingProblemCandidate>& candidates,
    const TrainingGoal& goal
) {
    BruteForceTrainingPlan result;
    if (candidates.empty() || goal.target_count <= 0 || goal.time_budget_minutes <= 0) {
        result.optimized.uncovered_tags = unique_goal_tags(goal);
        return result;
    }

    const auto sorted = sorted_candidates(candidates);
    std::vector<TrainingProblemCandidate> eligible;
    for (const auto& candidate : sorted) {
        if (candidate.estimated_minutes > 0 &&
            candidate.estimated_minutes <= goal.time_budget_minutes) {
            eligible.push_back(candidate);
        }
    }

    result.considered_candidate_count = static_cast<int>(eligible.size());
    if (eligible.empty()) {
        result.optimized.uncovered_tags = unique_goal_tags(goal);
        return result;
    }

    const auto search_window = selectStableDifficultyWindow(eligible, goal);
    const auto& search_candidates = search_window.candidates.empty()
        ? eligible
        : search_window.candidates;
    const auto target_tags = unique_goal_tags(goal);
    const int center_difficulty =
        (search_candidates.front().difficulty + search_candidates.back().difficulty) / 2;

    result.optimized = optimize_candidates_by_sparse_dp(
        search_candidates,
        goal,
        target_tags,
        center_difficulty,
        brute_force_selection_reason,
        &result.evaluated_combination_count
    );
    return result;
}

} // namespace training
