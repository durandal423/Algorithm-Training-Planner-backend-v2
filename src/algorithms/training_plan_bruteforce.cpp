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

std::string brute_force_selection_reason(
    const TrainingProblemCandidate& candidate,
    const std::vector<std::string>& target_tags,
    int score
) {
    std::string reason = "暴力枚举对照组；递归穷举候选题组合选中";
    if (!target_tags.empty()) {
        reason += "；覆盖目标标签=" + join_tags(target_tags);
    }
    if (candidate.wrong_attempt_count > 0) {
        reason += "；历史非AC次数=" + std::to_string(candidate.wrong_attempt_count);
    }
    reason += "；score=" + std::to_string(score);
    return reason;
}

std::vector<TrainingProblemCandidate> sorted_eligible_candidates(
    const std::vector<TrainingProblemCandidate>& candidates,
    const TrainingGoal& goal
) {
    std::vector<TrainingProblemCandidate> eligible;
    for (const auto& candidate : candidates) {
        if (candidate.estimated_minutes > 0 &&
            candidate.estimated_minutes <= goal.time_budget_minutes) {
            eligible.push_back(candidate);
        }
    }

    std::stable_sort(eligible.begin(), eligible.end(), [&goal](const auto& lhs, const auto& rhs) {
        if (lhs.estimated_minutes != rhs.estimated_minutes) {
            return lhs.estimated_minutes < rhs.estimated_minutes;
        }
        if (lhs.matched_tag_count != rhs.matched_tag_count) {
            return lhs.matched_tag_count > rhs.matched_tag_count;
        }
        if (goal.prefer_wrong_problems && lhs.wrong_attempt_count != rhs.wrong_attempt_count) {
            return lhs.wrong_attempt_count > rhs.wrong_attempt_count;
        }
        if (lhs.difficulty != rhs.difficulty) {
            return lhs.difficulty < rhs.difficulty;
        }
        return lhs.id < rhs.id;
    });
    return eligible;
}

int brute_force_search_size(int eligible_count, int target_count) {
    if (eligible_count <= 0 || target_count <= 0) {
        return 0;
    }

    const int max_count = std::min(eligible_count, target_count);
    int desired = max_count + 6;
    if (max_count <= 12) {
        desired = std::max(desired, 20);
    }
    if (max_count >= 24) {
        desired = max_count + 4;
    }
    return std::min(eligible_count, std::max(max_count, desired));
}

struct CandidateScore {
    int score{};
    int mask{};
};

struct BestCombination {
    bool found{};
    int score{kNegativeInfinity};
    int minutes{};
    int mask{};
    std::vector<int> indices;
};

bool better_combination(int score, int mask, int minutes, const BestCombination& best) {
    if (!best.found) {
        return true;
    }
    if (score != best.score) {
        return score > best.score;
    }
    if (bit_count(mask) != bit_count(best.mask)) {
        return bit_count(mask) > bit_count(best.mask);
    }
    return minutes < best.minutes;
}

void enumerate_exact_count(
    const std::vector<TrainingProblemCandidate>& candidates,
    const std::vector<CandidateScore>& scores,
    int budget,
    int start,
    int remaining,
    int current_minutes,
    int current_score,
    int current_mask,
    std::vector<int>& current_indices,
    BestCombination& best,
    long long& evaluated_count
) {
    if (remaining == 0) {
        ++evaluated_count;
        if (better_combination(current_score, current_mask, current_minutes, best)) {
            best.found = true;
            best.score = current_score;
            best.minutes = current_minutes;
            best.mask = current_mask;
            best.indices = current_indices;
        }
        return;
    }

    const int candidate_count = static_cast<int>(candidates.size());
    if (candidate_count - start < remaining) {
        return;
    }

    for (int i = start; i <= candidate_count - remaining; ++i) {
        const int next_minutes = current_minutes + candidates[i].estimated_minutes;
        if (next_minutes > budget) {
            continue;
        }

        current_indices.push_back(i);
        enumerate_exact_count(
            candidates,
            scores,
            budget,
            i + 1,
            remaining - 1,
            next_minutes,
            current_score + scores[i].score,
            current_mask | scores[i].mask,
            current_indices,
            best,
            evaluated_count
        );
        current_indices.pop_back();
    }
}

} // namespace

BruteForceTrainingPlan optimizeTrainingPlanByBruteForce(
    const std::vector<TrainingProblemCandidate>& candidates,
    const TrainingGoal& goal
) {
    BruteForceTrainingPlan result;
    const auto requested_tags = unique_goal_tags(goal);
    if (candidates.empty() || goal.target_count <= 0 || goal.time_budget_minutes <= 0) {
        result.optimized.uncovered_tags = requested_tags;
        return result;
    }

    auto eligible = sorted_eligible_candidates(candidates, goal);
    const int search_size = brute_force_search_size(
        static_cast<int>(eligible.size()),
        goal.target_count
    );
    result.considered_candidate_count = search_size;
    if (search_size <= 0) {
        result.optimized.uncovered_tags = requested_tags;
        return result;
    }

    eligible.resize(search_size);
    const auto [min_difficulty, max_difficulty] = std::minmax_element(
        eligible.begin(),
        eligible.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.difficulty < rhs.difficulty;
        }
    );
    const int center_difficulty = (min_difficulty->difficulty + max_difficulty->difficulty) / 2;
    const auto mask_tags = mask_tags_for_goal(goal);
    const auto tag_index = tag_index_map(mask_tags);
    const auto requested = tag_set(requested_tags);

    std::vector<CandidateScore> scores;
    scores.reserve(eligible.size());
    for (const auto& candidate : eligible) {
        scores.push_back(CandidateScore{
            .score = scoreProblemForTrainingGoal(candidate, goal, center_difficulty),
            .mask = candidate_mask(candidate, tag_index)
        });
    }

    BestCombination best;
    const int max_selection_count = std::min<int>(goal.target_count, eligible.size());
    std::vector<int> current_indices;
    for (int count = max_selection_count; count >= 1; --count) {
        BestCombination best_for_count;
        enumerate_exact_count(
            eligible,
            scores,
            goal.time_budget_minutes,
            0,
            count,
            0,
            0,
            0,
            current_indices,
            best_for_count,
            result.evaluated_combination_count
        );
        if (best_for_count.found) {
            best = std::move(best_for_count);
            break;
        }
    }

    if (!best.found) {
        result.optimized.uncovered_tags = requested_tags;
        return result;
    }

    result.optimized.total_score = std::max(0, best.score);
    result.optimized.total_estimated_minutes = best.minutes;
    result.optimized.covered_tag_mask = best.mask;
    result.optimized.covered_tags = tags_for_mask(best.mask, mask_tags);
    result.optimized.uncovered_tags = uncovered_tags(requested_tags, result.optimized.covered_tags);

    for (const int index : best.indices) {
        const auto& candidate = eligible[index];
        const auto target_tags = covered_target_tags(candidate, requested);
        result.optimized.items.push_back(SelectedTrainingProblem{
            .problem = candidate,
            .score = scores[index].score,
            .covered_target_tags = target_tags,
            .selected_reason = brute_force_selection_reason(candidate, target_tags, scores[index].score)
        });
    }

    std::stable_sort(result.optimized.items.begin(), result.optimized.items.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.problem.difficulty != rhs.problem.difficulty) {
            return lhs.problem.difficulty < rhs.problem.difficulty;
        }
        return lhs.problem.id < rhs.problem.id;
    });
    return result;
}

} // namespace training
