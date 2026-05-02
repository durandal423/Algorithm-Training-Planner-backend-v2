#include "api/http_json.hpp"

namespace api {

crow::response json_response(int status, const nlohmann::json& body) {
    crow::response response{status};
    response.set_header("Content-Type", "application/json");
    response.write(body.dump());
    return response;
}

std::string required_string(const nlohmann::json& body, const char* key) {
    if (!body.contains(key) || !body.at(key).is_string()) {
        throw std::runtime_error{std::string{"missing string field: "} + key};
    }
    return body.at(key).get<std::string>();
}

std::vector<std::string> string_array(
    const nlohmann::json& body,
    const char* key
) {
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

std::optional<std::string> optional_query_string(
    const crow::request& req,
    const char* key
) {
    if (const char* value = req.url_params.get(key)) {
        return value;
    }
    return std::nullopt;
}

std::optional<int> optional_query_int(const crow::request& req, const char* key) {
    const auto value = optional_query_string(req, key);
    if (!value) {
        return std::nullopt;
    }
    return std::stoi(*value);
}

std::optional<long long> optional_query_long(
    const crow::request& req,
    const char* key
) {
    const auto value = optional_query_string(req, key);
    if (!value) {
        return std::nullopt;
    }
    return std::stoll(*value);
}

} // namespace api
