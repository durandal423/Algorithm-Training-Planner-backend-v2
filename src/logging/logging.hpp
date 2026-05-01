#pragma once

#include <chrono>
#include <string>

#include <crow.h>

struct OptionsLogger {
    struct context {};

    void before_handle(crow::request& req, crow::response& res, context& ctx);
    void after_handle(crow::request& req, crow::response& res, context& ctx) {}
};

struct AccessLogger {
    struct context {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    };

    void before_handle(crow::request& req, crow::response& res, context& ctx);
    void after_handle(crow::request& req, crow::response& res, context& ctx);
};

class CrowSpdlogHandler final : public crow::ILogHandler {
public:
    void log(std::string message, crow::LogLevel level) override;
};

void initSpdlog();
void installCrowLogger();
