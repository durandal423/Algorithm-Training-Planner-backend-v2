#include "domain/judge/sandbox_runner.hpp"

#include "domain/judge/docker_sandbox_pool.hpp"
#include "domain/judge/sandbox_process.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace judge {
namespace {

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::string expand_template(
    std::string command_template,
    const std::string& source,
    const std::string& executable,
    const std::string& input,
    const std::string& stdout_file,
    const std::string& stderr_file
) {
    command_template = replace_all(std::move(command_template), "{source}", source);
    command_template = replace_all(std::move(command_template), "{executable}", executable);
    command_template = replace_all(std::move(command_template), "{input}", input);
    command_template = replace_all(std::move(command_template), "{stdout}", stdout_file);
    command_template = replace_all(std::move(command_template), "{stderr}", stderr_file);
    return command_template;
}

bool files_exceed_limit(
    const std::filesystem::path& stdout_path,
    const std::filesystem::path& stderr_path,
    std::uintmax_t limit_bytes
) {
    std::error_code out_error;
    std::error_code err_error;
    const auto out_size = std::filesystem::file_size(stdout_path, out_error);
    const auto err_size = std::filesystem::file_size(stderr_path, err_error);
    return (out_error ? 0 : out_size) + (err_error ? 0 : err_size) > limit_bytes;
}

bool docker_runner_error(const sandbox_process::CommandCapture& capture) {
    if (capture.process.system_error) {
        return true;
    }
    const auto text = capture.stderr_text + "\n" + capture.stdout_text;
    return text.find("OCI runtime exec failed") != std::string::npos ||
        text.find("unable to start container process") != std::string::npos ||
        text.find("Error response from daemon") != std::string::npos ||
        text.find("No such container") != std::string::npos ||
        text.find("is not running") != std::string::npos;
}

std::string trim_copy(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' ||
                             text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }
    std::size_t first = 0;
    while (first < text.size() && (text[first] == '\n' || text[first] == '\r' ||
                                   text[first] == ' ' || text[first] == '\t')) {
        ++first;
    }
    return first == 0 ? text : text.substr(first);
}

std::string lower_copy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::optional<bool> container_oom_killed(const std::string& container_name) {
    const auto capture = sandbox_process::runCommandCapture(
        "docker inspect --format " + sandbox_process::quoteShell("{{.State.OOMKilled}}") +
            " " + sandbox_process::quoteShell(container_name),
        5000
    );
    if (!sandbox_process::processSucceeded(capture.process)) {
        return std::nullopt;
    }
    const auto text = trim_copy(capture.stdout_text);
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    return std::nullopt;
}

bool memory_limit_error_text(const std::string& text) {
    const auto lowered = lower_copy(text);
    return lowered.find("std::bad_alloc") != std::string::npos ||
        lowered.find("cannot allocate memory") != std::string::npos ||
        lowered.find("memory allocation failed") != std::string::npos ||
        lowered.find("out of memory") != std::string::npos ||
        lowered.find("killed") != std::string::npos;
}

bool memory_limit_error_text(const sandbox_process::CommandCapture& capture) {
    return memory_limit_error_text(capture.stderr_text + "\n" + capture.stdout_text);
}

bool memory_limit_over_reported_usage(int memory_kb, int memory_limit_mb) {
    if (memory_kb <= 0 || memory_limit_mb <= 0) {
        return false;
    }
    return static_cast<long long>(memory_kb) >
        static_cast<long long>(memory_limit_mb) * 1024;
}

bool memory_limit_exceeded(
    const sandbox_process::CommandCapture& capture,
    const std::string& container_name,
    const std::string& user_stderr,
    int memory_kb,
    int memory_limit_mb
) {
    if (capture.process.timed_out || capture.process.exit_code == 124) {
        return false;
    }

    if (const auto oom_killed = container_oom_killed(container_name); oom_killed.value_or(false)) {
        return true;
    }

    if (capture.process.exit_code == 137) {
        return true;
    }

    if (memory_limit_over_reported_usage(memory_kb, memory_limit_mb)) {
        return true;
    }

    return capture.process.exit_code != 0 &&
        (memory_limit_error_text(capture) || memory_limit_error_text(user_stderr));
}

std::string measured_run_command(
    const std::string& command,
    const std::string& memory_file
) {
    return std::string{"if command -v /usr/bin/time >/dev/null 2>&1; then "} +
        "/usr/bin/time -f " + sandbox_process::quoteShell("OJ_MAX_RSS_KB=%M") +
        " -o " + sandbox_process::quoteShell(memory_file) +
        " sh -lc " + sandbox_process::quoteShell(command) +
        "; else sh -lc " + sandbox_process::quoteShell(command) + "; fi";
}

int parse_memory_kb(const std::filesystem::path& memory_path) {
    const auto text = sandbox_process::readLimited(memory_path, 4096);
    const std::string marker = "OJ_MAX_RSS_KB=";
    const auto marker_pos = text.find(marker);
    if (marker_pos == std::string::npos) {
        return 0;
    }

    auto pos = marker_pos + marker.size();
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }

    long long value = 0;
    bool has_digit = false;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
        has_digit = true;
        value = value * 10 + (text[pos] - '0');
        if (value > std::numeric_limits<int>::max()) {
            return std::numeric_limits<int>::max();
        }
        ++pos;
    }

    return has_digit ? static_cast<int>(value) : 0;
}

std::string default_compile_template() {
    return "g++ -std=c++17 -O2 -pipe {source} -o {executable}";
}

std::string default_run_template() {
    return "{executable} < {input} > {stdout} 2> {stderr}";
}

} // namespace

SandboxRunner::SandboxRunner(int pool_slot)
    : pool_slot_(pool_slot) {}

void SandboxRunner::ensureDockerPool() {
    docker_sandbox_pool::ensurePool();
}

CompileResult SandboxRunner::compile(
    const Submission& submission,
    const JudgeConfig& config,
    const std::filesystem::path& work_dir
) const {
    std::filesystem::create_directories(work_dir);
    const auto source_path = work_dir / "main.cpp";
    {
        std::ofstream source{source_path, std::ios::binary};
        source << submission.source_code;
    }

    const auto container_dir = docker_sandbox_pool::containerWorkDirFor(work_dir);
    const auto container_name = containerName();
    docker_sandbox_pool::ensureContainerCanSeeWorkDir(container_name, work_dir);
    const auto inner = expand_template(
        config.compile_command_template.value_or(default_compile_template()),
        container_dir + "/main.cpp",
        container_dir + "/main",
        {},
        {},
        {}
    );
    const auto command = docker_sandbox_pool::dockerExecCommand(
        container_name,
        container_dir,
        "timeout 15s sh -lc " + sandbox_process::quoteShell(inner)
    );
    const auto capture = sandbox_process::runCommandCapture(command, 20000);
    const auto system_error = docker_runner_error(capture);
    return CompileResult{
        .success = sandbox_process::processSucceeded(capture.process),
        .timed_out = capture.process.timed_out || capture.process.exit_code == 124,
        .system_error = system_error,
        .exit_code = capture.process.exit_code,
        .time_ms = capture.process.time_ms,
        .stdout_sample = capture.stdout_text,
        .stderr_sample = capture.stderr_text,
        .message = capture.process.message
    };
}

RunResult SandboxRunner::run(
    const JudgeConfig& config,
    const JudgeTestCase& test_case,
    const std::filesystem::path& work_dir,
    int run_index
) const {
    std::filesystem::create_directories(work_dir);
    const auto input_path = work_dir / ("input-" + std::to_string(run_index) + ".txt");
    const auto stdout_path = work_dir / ("stdout-" + std::to_string(run_index) + ".txt");
    const auto stderr_path = work_dir / ("stderr-" + std::to_string(run_index) + ".txt");
    const auto memory_path = work_dir / ("memory-" + std::to_string(run_index) + ".txt");
    {
        std::ofstream input{input_path, std::ios::binary};
        input << test_case.input_data;
    }
    std::filesystem::remove(memory_path);

    const auto container_dir = docker_sandbox_pool::containerWorkDirFor(work_dir);
    const auto container_name = containerName();
    docker_sandbox_pool::ensureContainerCanSeeWorkDir(container_name, work_dir);
    const auto docker_input = container_dir + "/" + input_path.filename().string();
    const auto docker_stdout = container_dir + "/" + stdout_path.filename().string();
    const auto docker_stderr = container_dir + "/" + stderr_path.filename().string();
    const auto docker_memory = container_dir + "/" + memory_path.filename().string();
    const auto time_limit_ms = test_case.time_limit_ms.value_or(config.time_limit_ms);
    const auto memory_limit_mb = test_case.memory_limit_mb.value_or(config.memory_limit_mb);
    const auto timeout_seconds = std::max(1, (time_limit_ms + 999) / 1000);

    auto inner = expand_template(
        config.run_command_template.value_or(default_run_template()),
        container_dir + "/main.cpp",
        container_dir + "/main",
        docker_input,
        docker_stdout,
        docker_stderr
    );
    inner = measured_run_command(inner, docker_memory);
    inner = "ulimit -v " + std::to_string(static_cast<long long>(memory_limit_mb) * 1024) +
        " 2>/dev/null || true; timeout " + std::to_string(timeout_seconds) +
        "s sh -lc " + sandbox_process::quoteShell(inner);

    const auto command = docker_sandbox_pool::dockerExecCommand(container_name, container_dir, inner);
    const auto capture = sandbox_process::runCommandCapture(command, time_limit_ms + 5000);
    const auto system_error = docker_runner_error(capture);
    const auto output_limit_exceeded = files_exceed_limit(
        stdout_path,
        stderr_path,
        static_cast<std::uintmax_t>(config.output_limit_kb) * 1024
    );
    const int memory_kb = parse_memory_kb(memory_path);
    const auto stdout_sample = sandbox_process::readLimited(stdout_path, 64 * 1024);
    const auto stderr_sample = sandbox_process::readLimited(stderr_path, 64 * 1024);

    return RunResult{
        .timed_out = capture.process.timed_out || capture.process.exit_code == 124,
        .output_limit_exceeded = output_limit_exceeded,
        .memory_limit_exceeded = memory_limit_exceeded(
            capture,
            container_name,
            stderr_sample,
            memory_kb,
            memory_limit_mb
        ),
        .system_error = system_error,
        .exit_code = capture.process.exit_code,
        .time_ms = capture.process.time_ms,
        .memory_kb = memory_kb,
        .stdout_sample = stdout_sample,
        .stderr_sample = stderr_sample,
        .message = capture.process.message.empty() ? capture.stderr_text : capture.process.message
    };
}

std::string SandboxRunner::containerName() const {
    return docker_sandbox_pool::containerNameForSlot(pool_slot_);
}

} // namespace judge
