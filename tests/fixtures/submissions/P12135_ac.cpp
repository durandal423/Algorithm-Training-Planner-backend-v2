#include <iostream>
#include <string>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string top;
    std::string bottom;
    std::getline(std::cin, top);
    std::getline(std::cin, bottom);

    int last = -1;
    int answer = 0;
    for (int i = 0; i < static_cast<int>(top.size()); ++i) {
        if (top[i] == '#' || bottom[i] == '#') {
            last = i;
            break;
        }
    }
    if (last < 0) {
        std::cout << 0 << '\n';
        return 0;
    }

    for (int i = last + 1; i < static_cast<int>(top.size()); ++i) {
        if (top[i] != '#' && bottom[i] != '#') {
            continue;
        }

        if ((top[last] == '#' && top[i] == '#') ||
            (bottom[last] == '#' && bottom[i] == '#')) {
            answer += (i - last) - 1;
        } else {
            answer += i - last;
            top[i] = '#';
            bottom[i] = '#';
        }
        last = i;
    }

    std::cout << answer << '\n';
    return 0;
}
