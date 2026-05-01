#include "domain/judge/sandbox_runner.hpp"

#include "config.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace judge {
namespace {

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

std::string env_string(const char* key) {
    const char* value = std::getenv(key);
    return value == nullptr ? std::string{} : std::string{value};
}

std::string quote_shell(const std::string& value) {
    std::string quoted = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

std::string quote_go_string(const std::string& value) {
    std::string quoted = "\"";
    for (const char ch : value) {
        if (ch == '"' || ch == '\\') {
            quoted.push_back('\\');
        }
        quoted.push_back(ch);
    }
    quoted.push_back('"');
    return quoted;
}

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

std::filesystem::path normalized_path(const std::filesystem::path& path) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return normalized;
    }
    normalized = std::filesystem::absolute(path, error);
    return error ? path : normalized;
}

bool is_same_or_child_path(const std::filesystem::path& child, const std::filesystem::path& parent) {
    std::error_code error;
    const auto relative = std::filesystem::relative(child, parent, error);
    if (error) {
        return false;
    }
    const auto text = relative.generic_string();
    return text == "." || (text != ".." && text.rfind("../", 0) != 0);
}

std::string read_limited(const std::filesystem::path& path, std::size_t limit) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return {};
    }
    std::string text(limit, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(input.gcount()));
    return text;
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

ProcessResult run_process(
    const std::string& command,
    int timeout_ms,
    const std::filesystem::path& stdin_path,
    const std::filesystem::path& stdout_path,
    const std::filesystem::path& stderr_path
) {
    const auto started = std::chrono::steady_clock::now();
    const pid_t pid = fork();
    if (pid < 0) {
        return {.system_error = true, .exit_code = -1, .message = "failed to fork"};
    }

    if (pid == 0) {
        setpgid(0, 0);
        const int in_fd = open(stdin_path.c_str(), O_RDONLY);
        const int out_fd = open(stdout_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
        const int err_fd = open(stderr_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (in_fd < 0 || out_fd < 0 || err_fd < 0) {
            _exit(127);
        }
        dup2(in_fd, STDIN_FILENO);
        dup2(out_fd, STDOUT_FILENO);
        dup2(err_fd, STDERR_FILENO);
        close(in_fd);
        close(out_fd);
        close(err_fd);
        execl("/bin/sh", "sh", "-lc", command.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    bool timed_out = false;
    while (true) {
        const pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid) {
            break;
        }
        if (done < 0) {
            return {.system_error = true, .exit_code = -1, .message = "failed to wait for process"};
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started
        ).count();
        if (elapsed > timeout_ms) {
            timed_out = true;
            kill(-pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    int exit_code = 0;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exit_code = 128 + WTERMSIG(status);
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started
    ).count();
    return {
        .timed_out = timed_out,
        .system_error = false,
        .exit_code = exit_code,
        .time_ms = static_cast<int>(elapsed)
    };
}

std::filesystem::path temp_command_dir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        ("oj-command-" + std::to_string(stamp) + "-" + std::to_string(::getpid()));
}

CommandCapture run_command_capture(const std::string& command, int timeout_ms = 30000) {
    const auto dir = temp_command_dir();
    std::filesystem::create_directories(dir);
    const auto stdin_path = dir / "stdin.txt";
    const auto stdout_path = dir / "stdout.txt";
    const auto stderr_path = dir / "stderr.txt";
    {
        std::ofstream input{stdin_path, std::ios::binary};
    }

    const auto process = run_process(command, timeout_ms, stdin_path, stdout_path, stderr_path);
    CommandCapture capture{
        .process = process,
        .stdout_text = read_limited(stdout_path, 512 * 1024),
        .stderr_text = read_limited(stderr_path, 512 * 1024)
    };
    std::filesystem::remove_all(dir);
    return capture;
}

bool process_succeeded(const ProcessResult& result) {
    return !result.timed_out && !result.system_error && result.exit_code == 0;
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

void run_or_throw(const std::string& command, const std::string& action, int timeout_ms = 30000) {
    const auto capture = run_command_capture(command, timeout_ms);
    if (process_succeeded(capture.process)) {
        return;
    }
    const auto details = trim_copy(capture.stderr_text.empty() ? capture.stdout_text : capture.stderr_text);
    throw std::runtime_error(action + " failed" + (details.empty() ? std::string{} : ": " + details));
}

std::optional<std::string> docker_inspect_format(const std::string& target, const std::string& format) {
    const auto capture = run_command_capture(
        "docker inspect --format " + quote_shell(format) + " " + quote_shell(target),
        5000
    );
    if (!process_succeeded(capture.process)) {
        return std::nullopt;
    }
    return trim_copy(capture.stdout_text);
}

std::string docker_image() {
    const auto override = env_string("ATP_JUDGE_DOCKER_IMAGE");
    return override.empty() ? "gcc:13" : override;
}

std::filesystem::path work_root() {
    return normalized_path(config::JUDGE_DOCKER_WORK_ROOT);
}

std::filesystem::path resolve_docker_host_path(const std::filesystem::path& container_path) {
    const auto override = env_string("ATP_JUDGE_DOCKER_HOST_WORK_ROOT");
    if (!override.empty()) {
        return override;
    }

    const auto target = normalized_path(container_path);
    const auto hostname = env_string("HOSTNAME");
    if (hostname.empty()) {
        return target;
    }

    const auto mounts_text = docker_inspect_format(hostname, "{{json .Mounts}}");
    if (!mounts_text || mounts_text->empty()) {
        return target;
    }

    try {
        const auto mounts = nlohmann::json::parse(*mounts_text);
        std::filesystem::path best_source;
        std::filesystem::path best_destination;
        std::size_t best_length = 0;
        for (const auto& mount : mounts) {
            if (!mount.contains("Source") || !mount.contains("Destination")) {
                continue;
            }
            const auto destination = normalized_path(mount.at("Destination").get<std::string>());
            if (!is_same_or_child_path(target, destination)) {
                continue;
            }
            const auto length = destination.generic_string().size();
            if (length <= best_length) {
                continue;
            }
            best_length = length;
            best_destination = destination;
            best_source = mount.at("Source").get<std::string>();
        }
        if (!best_source.empty()) {
            std::error_code error;
            const auto relative = std::filesystem::relative(target, best_destination, error);
            if (!error && relative != ".") {
                return best_source / relative;
            }
            return best_source;
        }
    } catch (const std::exception& error) {
        spdlog::warn("failed to resolve Docker host work root: {}", error.what());
    }
    return target;
}

std::string container_work_dir_for(const std::filesystem::path& work_dir) {
    const auto root = work_root();
    const auto work = normalized_path(work_dir);
    std::error_code error;
    const auto relative = std::filesystem::relative(work, root, error);
    const auto text = error ? std::string{} : relative.generic_string();
    if (error || text == ".." || text.rfind("../", 0) == 0) {
        throw std::runtime_error{"judge work directory must be under JUDGE_DOCKER_WORK_ROOT"};
    }
    return text == "." ? "/workspace" : "/workspace/" + text;
}

std::string docker_exec_command(
    const std::string& container_name,
    const std::string& container_work_dir,
    const std::string& inner
) {
    return "docker exec -i -w " + quote_shell(container_work_dir) + " " +
        quote_shell(container_name) + " sh -lc " + quote_shell(inner);
}

std::string container_name_for_slot(int slot) {
    return std::string{config::JUDGE_DOCKER_POOL_NAME_PREFIX} + "-" + std::to_string(slot);
}

void create_container(
    const std::string& name,
    const std::string& image,
    const std::filesystem::path& host_work_root
) {
    std::ostringstream command;
    command << "docker run -d"
            << " --name " << quote_shell(name)
            << " --label " << quote_shell("oj.judge.pool=1")
            << " --label " << quote_shell("oj.judge.work-root=" + host_work_root.string())
            << " --label " << quote_shell("oj.judge.image=" + image)
            << " --network none"
            << " --memory " << config::JUDGE_DOCKER_POOL_MEMORY_MB << "m"
            << " --cpus 1"
            << " --pids-limit 128"
            << " -v " << quote_shell(host_work_root.string() + ":/workspace")
            << " -w /workspace "
            << quote_shell(image)
            << " sh -lc " << quote_shell("while true; do sleep 3600; done");
    run_or_throw(command.str(), "create Docker judge pool container", 300000);
}

std::string container_label(const std::string& name, const std::string& label) {
    return docker_inspect_format(name, "{{index .Config.Labels " + quote_go_string(label) + "}}")
        .value_or(std::string{});
}

std::string first_container_label(
    const std::string& name,
    const std::string& primary,
    const std::string& legacy
) {
    const auto value = container_label(name, primary);
    if (!value.empty()) {
        return value;
    }
    return container_label(name, legacy);
}

void ensure_container(
    const std::string& name,
    const std::string& image,
    const std::filesystem::path& host_work_root
) {
    if (!docker_inspect_format(name, "{{.Id}}")) {
        create_container(name, image, host_work_root);
        return;
    }

    const bool managed =
        container_label(name, "oj.judge.pool") == "1" ||
        container_label(name, "atp.judge.pool") == "1";
    if (!managed) {
        throw std::runtime_error{"Docker container '" + name + "' already exists and is not an OJ judge container"};
    }

    const auto actual_work_root = first_container_label(
        name,
        "oj.judge.work-root",
        "atp.judge.work-root"
    );
    const auto actual_image = first_container_label(name, "oj.judge.image", "atp.judge.image");
    if (actual_work_root != host_work_root.string() || actual_image != image) {
        run_or_throw("docker rm -f " + quote_shell(name), "remove stale Docker judge pool container");
        create_container(name, image, host_work_root);
        return;
    }

    const auto running = docker_inspect_format(name, "{{.State.Running}}").value_or("false");
    if (running != "true") {
        run_or_throw("docker start " + quote_shell(name), "start Docker judge pool container");
    }
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
    static std::once_flag once;
    std::call_once(once, [] {
        if (config::JUDGE_DOCKER_POOL_SIZE <= 0) {
            throw std::runtime_error{"JUDGE_DOCKER_POOL_SIZE must be greater than 0"};
        }
        std::filesystem::create_directories(work_root());
        const auto image = docker_image();
        const auto host_work_root = resolve_docker_host_path(work_root());
        for (int slot = 0; slot < config::JUDGE_DOCKER_POOL_SIZE; ++slot) {
            ensure_container(container_name_for_slot(slot), image, host_work_root);
        }
        spdlog::info(
            "Docker judge pool ready (image={}, size={}, work_root={})",
            image,
            config::JUDGE_DOCKER_POOL_SIZE,
            host_work_root.string()
        );
    });
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

    const auto container_dir = container_work_dir_for(work_dir);
    const auto inner = expand_template(
        config.compile_command_template.value_or(default_compile_template()),
        container_dir + "/main.cpp",
        container_dir + "/main",
        {},
        {},
        {}
    );
    const auto command = docker_exec_command(
        containerName(),
        container_dir,
        "timeout 15s sh -lc " + quote_shell(inner)
    );
    const auto capture = run_command_capture(command, 20000);
    return CompileResult{
        .success = process_succeeded(capture.process),
        .timed_out = capture.process.timed_out || capture.process.exit_code == 124,
        .system_error = capture.process.system_error,
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
    {
        std::ofstream input{input_path, std::ios::binary};
        input << test_case.input_data;
    }

    const auto container_dir = container_work_dir_for(work_dir);
    const auto docker_input = container_dir + "/" + input_path.filename().string();
    const auto docker_stdout = container_dir + "/" + stdout_path.filename().string();
    const auto docker_stderr = container_dir + "/" + stderr_path.filename().string();
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
    inner = "ulimit -v " + std::to_string(static_cast<long long>(memory_limit_mb) * 1024) +
        " 2>/dev/null || true; timeout " + std::to_string(timeout_seconds) +
        "s sh -lc " + quote_shell(inner);

    const auto command = docker_exec_command(containerName(), container_dir, inner);
    const auto capture = run_command_capture(command, time_limit_ms + 5000);
    const auto output_limit_exceeded = files_exceed_limit(
        stdout_path,
        stderr_path,
        static_cast<std::uintmax_t>(config.output_limit_kb) * 1024
    );

    return RunResult{
        .timed_out = capture.process.timed_out || capture.process.exit_code == 124,
        .output_limit_exceeded = output_limit_exceeded,
        .system_error = capture.process.system_error,
        .exit_code = capture.process.exit_code,
        .time_ms = capture.process.time_ms,
        .memory_kb = 0,
        .stdout_sample = read_limited(stdout_path, 64 * 1024),
        .stderr_sample = read_limited(stderr_path, 64 * 1024),
        .message = capture.process.message.empty() ? capture.stderr_text : capture.process.message
    };
}

std::string SandboxRunner::containerName() const {
    return container_name_for_slot(pool_slot_);
}

} // namespace judge
