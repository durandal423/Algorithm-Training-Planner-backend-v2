#pragma once

#include <crow.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace api {

crow::response json_response(int status, const nlohmann::json& body);

template <typename T>
T required_value(const nlohmann::json& body, const char* key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        throw std::runtime_error{std::string{"missing field: "} + key};
    }
    return body.at(key).get<T>();
}

std::string required_string(const nlohmann::json& body, const char* key);

inline int required_int(const nlohmann::json& body, const char* key) {
    return required_value<int>(body, key);
}

inline long long required_long(const nlohmann::json& body, const char* key) {
    return required_value<long long>(body, key);
}

std::vector<std::string> string_array(
    const nlohmann::json& body,
    const char* key
);

std::optional<std::string> optional_query_string(
    const crow::request& req,
    const char* key
);

std::optional<int> optional_query_int(const crow::request& req, const char* key);

std::optional<long long> optional_query_long(
    const crow::request& req,
    const char* key
);

inline std::string trim_copy(std::string text) {
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string{first, last};
}

} // namespace api
