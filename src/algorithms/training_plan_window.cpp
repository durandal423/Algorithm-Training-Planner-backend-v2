#include "algorithms/training_plan_algorithms.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace training {
namespace {

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

int desired_window_size(int candidate_count, int target_count) {
    if (candidate_count <= 0 || target_count <= 0) {
        return 0;
    }
    return std::min(candidate_count, std::max(target_count, target_count * 3));
}

struct GoalTagInfo {
    std::vector<std::string> requested;
    std::unordered_map<std::string, int> index_by_name;
    std::vector<bool> available;
    int available_count{};
};

GoalTagInfo build_goal_tag_info(
    const std::vector<TrainingProblemCandidate>& candidates,
    const TrainingGoal& goal
) {
    GoalTagInfo info;
    info.requested = unique_goal_tags(goal);
    info.available.assign(info.requested.size(), false);

    for (int i = 0; i < static_cast<int>(info.requested.size()); ++i) {
        info.index_by_name[info.requested[i]] = i;
    }

    for (const auto& candidate : candidates) {
        for (const auto& tag : candidate.tag_names) {
            const auto iter = info.index_by_name.find(tag);
            if (iter == info.index_by_name.end()) {
                continue;
            }
            if (!info.available[iter->second]) {
                info.available[iter->second] = true;
                ++info.available_count;
            }
        }
    }

    return info;
}

struct WindowStats {
    int wrong_count{};
    int covered_available_count{};
    std::vector<int> tag_counts;
};

void add_candidate(
    WindowStats& stats,
    const TrainingProblemCandidate& candidate,
    const GoalTagInfo& tags
) {
    if (candidate.wrong_attempt_count > 0) {
        ++stats.wrong_count;
    }

    for (const auto& tag : candidate.tag_names) {
        const auto iter = tags.index_by_name.find(tag);
        if (iter == tags.index_by_name.end()) {
            continue;
        }

        const int index = iter->second;
        if (stats.tag_counts[index] == 0 && tags.available[index]) {
            ++stats.covered_available_count;
        }
        ++stats.tag_counts[index];
    }
}

void remove_candidate(
    WindowStats& stats,
    const TrainingProblemCandidate& candidate,
    const GoalTagInfo& tags
) {
    if (candidate.wrong_attempt_count > 0) {
        --stats.wrong_count;
    }

    for (const auto& tag : candidate.tag_names) {
        const auto iter = tags.index_by_name.find(tag);
        if (iter == tags.index_by_name.end()) {
            continue;
        }

        const int index = iter->second;
        if (stats.tag_counts[index] <= 0) {
            continue;
        }
        --stats.tag_counts[index];
        if (stats.tag_counts[index] == 0 && tags.available[index]) {
            --stats.covered_available_count;
        }
    }
}

std::vector<std::string> covered_tags(
    const WindowStats& stats,
    const GoalTagInfo& tags
) {
    std::vector<std::string> covered;
    for (int i = 0; i < static_cast<int>(tags.requested.size()); ++i) {
        if (stats.tag_counts[i] > 0) {
            covered.push_back(tags.requested[i]);
        }
    }
    return covered;
}

std::vector<std::string> uncovered_tags(
    const GoalTagInfo& tags,
    const std::vector<std::string>& covered
) {
    std::unordered_set<std::string> covered_set{covered.begin(), covered.end()};
    std::vector<std::string> uncovered;
    for (const auto& tag : tags.requested) {
        if (!covered_set.contains(tag)) {
            uncovered.push_back(tag);
        }
    }
    return uncovered;
}

bool is_valid_window(const WindowStats& stats, int size, int min_size, const GoalTagInfo& tags) {
    return size >= min_size && stats.covered_available_count == tags.available_count;
}

StableDifficultyWindow make_window(
    const std::vector<TrainingProblemCandidate>& sorted,
    int left,
    int right,
    const WindowStats& stats,
    const GoalTagInfo& tags
) {
    StableDifficultyWindow window;
    window.left_index = left;
    window.right_index = right;
    window.left_difficulty = sorted[left].difficulty;
    window.right_difficulty = sorted[right].difficulty;
    window.difficulty_span = window.right_difficulty - window.left_difficulty;
    window.wrong_problem_count = stats.wrong_count;
    window.covered_tags = covered_tags(stats, tags);
    window.uncovered_tags = uncovered_tags(tags, window.covered_tags);
    window.coverage_ratio = tags.available_count == 0
        ? 1.0
        : static_cast<double>(window.covered_tags.size()) / static_cast<double>(tags.available_count);
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

bool better_partial_window(
    const WindowStats& stats,
    int left,
    int right,
    const std::vector<TrainingProblemCandidate>& sorted,
    const StableDifficultyWindow& best,
    const TrainingGoal& goal
) {
    if (best.left_index < 0) {
        return true;
    }

    if (stats.covered_available_count != static_cast<int>(best.covered_tags.size())) {
        return stats.covered_available_count > static_cast<int>(best.covered_tags.size());
    }

    const int span = sorted[right].difficulty - sorted[left].difficulty;
    if (span != best.difficulty_span) {
        return span < best.difficulty_span;
    }

    if (goal.prefer_wrong_problems && stats.wrong_count != best.wrong_problem_count) {
        return stats.wrong_count > best.wrong_problem_count;
    }

    return sorted[left].id < sorted[best.left_index].id;
}

} // namespace

StableDifficultyWindow selectStableDifficultyWindow(
    const std::vector<TrainingProblemCandidate>& candidates,
    const TrainingGoal& goal
) {
    const auto sorted = sorted_candidates(candidates);
    if (sorted.empty() || goal.target_count <= 0) {
        return {};
    }

    const int min_size = desired_window_size(static_cast<int>(sorted.size()), goal.target_count);
    const auto tags = build_goal_tag_info(sorted, goal);
    StableDifficultyWindow best;
    WindowStats stats{
        .tag_counts = std::vector<int>(tags.requested.size(), 0)
    };
    int left = 0;

    for (int right = 0; right < static_cast<int>(sorted.size()); ++right) {
        add_candidate(stats, sorted[right], tags);

        while (right - left + 1 > min_size) {
            remove_candidate(stats, sorted[left], tags);
            ++left;
        }

        if (is_valid_window(stats, right - left + 1, min_size, tags)) {
            auto candidate = make_window(sorted, left, right, stats, tags);
            if (better_window(candidate, best, goal)) {
                best = std::move(candidate);
            }
        }
    }

    if (best.left_index >= 0) {
        return best;
    }

    StableDifficultyWindow fallback;
    WindowStats fallback_stats{
        .tag_counts = std::vector<int>(tags.requested.size(), 0)
    };
    left = 0;
    for (int right = 0; right < static_cast<int>(sorted.size()); ++right) {
        add_candidate(fallback_stats, sorted[right], tags);

        while (right - left + 1 > min_size) {
            remove_candidate(fallback_stats, sorted[left], tags);
            ++left;
        }

        if (right - left + 1 == min_size &&
            better_partial_window(fallback_stats, left, right, sorted, fallback, goal)) {
            fallback = make_window(sorted, left, right, fallback_stats, tags);
        }
    }

    if (fallback.left_index >= 0) {
        return fallback;
    }

    return {};
}

} // namespace training
