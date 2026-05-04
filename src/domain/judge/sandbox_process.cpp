#include "domain/judge/sandbox_process.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace judge::sandbox_process {
namespace {

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

} // namespace

std::string quoteShell(const std::string& value) {
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

std::string readLimited(const std::filesystem::path& path, std::size_t limit) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return {};
    }
    std::string text(limit, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(input.gcount()));
    return text;
}

CommandCapture runCommandCapture(const std::string& command, int timeout_ms) {
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
        .stdout_text = readLimited(stdout_path, 512 * 1024),
        .stderr_text = readLimited(stderr_path, 512 * 1024)
    };
    std::filesystem::remove_all(dir);
    return capture;
}

bool processSucceeded(const ProcessResult& result) {
    return !result.timed_out && !result.system_error && result.exit_code == 0;
}

} // namespace judge::sandbox_process
