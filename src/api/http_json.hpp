#pragma once

#include <crow.h>
#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace api {

crow::response json_response(int status, const nlohmann::json& body);

template <typename T>
std::optional<T> optional_value(const nlohmann::json& body, const char* key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        return std::nullopt;
    }
    return body.at(key).get<T>();
}

inline std::optional<std::string> optional_string(
    const nlohmann::json& body,
    const char* key
) {
    return optional_value<std::string>(body, key);
}

inline std::optional<int> optional_int(
    const nlohmann::json& body,
    const char* key
) {
    return optional_value<int>(body, key);
}

inline std::optional<long long> optional_long(
    const nlohmann::json& body,
    const char* key
) {
    return optional_value<long long>(body, key);
}

std::string required_string(const nlohmann::json& body, const char* key);

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

} // namespace api
