#include "logging/logging.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

void OptionsLogger::before_handle(crow::request& req, crow::response&, context&) {
    if (req.method == crow::HTTPMethod::Options) {
        spdlog::info("OPTIONS request: url={}, origin={}",
                     req.url,
                     req.get_header_value("Origin"));
    }
}


void AccessLogger::before_handle(crow::request&, crow::response&, context& ctx) {
    ctx.start = std::chrono::steady_clock::now();
}

void AccessLogger::after_handle(crow::request& req, crow::response& res, context& ctx) {
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - ctx.start
        ).count();

    spdlog::info("{} {} -> {} ({} ms)",
                 crow::method_name(req.method),
                 req.url,
                 res.code,
                 elapsed_ms);
}


void CrowSpdlogHandler::log(const std::string& message, crow::LogLevel level) {
    switch (level) {
        case crow::LogLevel::Debug:
            spdlog::debug("[crow] {}", message);
            break;
        case crow::LogLevel::Info:
            spdlog::info("[crow] {}", message);
            break;
        case crow::LogLevel::Warning:
            spdlog::warn("[crow] {}", message);
            break;
        case crow::LogLevel::Error:
            spdlog::error("[crow] {}", message);
            break;
        case crow::LogLevel::Critical:
            spdlog::critical("[crow] {}", message);
            break;
    }
}


void initSpdlog() {
    auto logger = spdlog::stdout_color_mt("console");

    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    logger->set_level(spdlog::level::debug);

    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
}


void installCrowLogger() {
    static CrowSpdlogHandler handler;

    crow::logger::setHandler(&handler);
    crow::logger::setLogLevel(crow::LogLevel::Info);
}
