#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace judge::sandbox_process {

struct ProcessResult {
    bool timed_out{};
    bool system_error{};
    int exit_code{};
    int time_ms{};
    std::string message;
};

struct CommandCapture {
    ProcessResult process;
    std::string stdout_text;
    std::string stderr_text;
};

std::string quoteShell(const std::string& value);
std::string readLimited(const std::filesystem::path& path, std::size_t limit);
CommandCapture runCommandCapture(const std::string& command, int timeout_ms = 30000);
bool processSucceeded(const ProcessResult& result);

} // namespace judge::sandbox_process
