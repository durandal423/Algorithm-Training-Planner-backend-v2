#include <algorithm>
#include <iostream>
#include <vector>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int n = 0;
    int m = 0;
    if (!(std::cin >> n >> m)) {
        return 0;
    }

    std::vector<long long> values(n);
    for (auto& value : values) {
        std::cin >> value;
    }
    std::sort(values.begin(), values.end());

    long long answer = -1;
    for (int i = 0; i + m <= n; ++i) {
        const long long low = values[i];
        const long long high = values[i + m - 1];
        const long long current = high * high - low * low;
        if (answer < 0 || current < answer) {
            answer = current;
        }
    }

    std::cout << answer << '\n';
    return 0;
}
