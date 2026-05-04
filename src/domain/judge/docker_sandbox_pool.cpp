#include "domain/judge/docker_sandbox_pool.hpp"

#include "config.hpp"
#include "domain/judge/sandbox_process.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace judge::docker_sandbox_pool {
namespace {

int pool_lock_fd = -1;

std::string env_string(const char* key) {
    const char* value = std::getenv(key);
    return value == nullptr ? std::string{} : std::string{value};
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
    const auto capture = sandbox_process::runCommandCapture(command, timeout_ms);
    if (sandbox_process::processSucceeded(capture.process)) {
        return;
    }
    const auto details = trim_copy(capture.stderr_text.empty() ? capture.stdout_text : capture.stderr_text);
    throw std::runtime_error(action + " failed" + (details.empty() ? std::string{} : ": " + details));
}

std::optional<std::string> docker_inspect_format(const std::string& target, const std::string& format) {
    const auto capture = sandbox_process::runCommandCapture(
        "docker inspect --format " + sandbox_process::quoteShell(format) + " " +
            sandbox_process::quoteShell(target),
        5000
    );
    if (!sandbox_process::processSucceeded(capture.process)) {
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

std::filesystem::path pool_lock_path() {
    std::string lock_name = config::JUDGE_DOCKER_POOL_NAME_PREFIX;
    for (char& ch : lock_name) {
        const bool safe = (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' ||
            ch == '_';
        if (!safe) {
            ch = '_';
        }
    }
    return std::filesystem::temp_directory_path() / (lock_name + ".docker-pool.lock");
}

void acquire_pool_lock() {
    const auto lock_path = pool_lock_path();
    const int fd = open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        throw std::runtime_error{
            "failed to open Docker judge pool lock: " + std::string{std::strerror(errno)}
        };
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        throw std::runtime_error{
            "Docker judge pool is already locked by another judge worker process"
        };
    }
    pool_lock_fd = fd;
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

void create_container(
    const std::string& name,
    const std::string& image,
    const std::filesystem::path& host_work_root
) {
    std::ostringstream command;
    command << "docker run -d"
            << " --name " << sandbox_process::quoteShell(name)
            << " --label " << sandbox_process::quoteShell("oj.judge.pool=1")
            << " --label " << sandbox_process::quoteShell("oj.judge.work-root=" + host_work_root.string())
            << " --label " << sandbox_process::quoteShell("oj.judge.image=" + image)
            << " --network none"
            << " --memory " << config::JUDGE_DOCKER_POOL_MEMORY_MB << "m"
            << " --cpus 1"
            << " --pids-limit 128"
            << " -v " << sandbox_process::quoteShell(host_work_root.string() + ":/workspace")
            << " -w /workspace "
            << sandbox_process::quoteShell(image)
            << " sh -lc " << sandbox_process::quoteShell("while true; do sleep 3600; done");
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
        run_or_throw(
            "docker rm -f " + sandbox_process::quoteShell(name),
            "remove stale Docker judge pool container"
        );
        create_container(name, image, host_work_root);
        return;
    }

    const auto running = docker_inspect_format(name, "{{.State.Running}}").value_or("false");
    if (running != "true") {
        run_or_throw("docker start " + sandbox_process::quoteShell(name), "start Docker judge pool container");
    }
}

bool container_path_exists(const std::string& container_name, const std::string& container_path) {
    const auto inner = "test -d " + sandbox_process::quoteShell(container_path);
    const auto command = "docker exec " + sandbox_process::quoteShell(container_name) +
        " sh -lc " + sandbox_process::quoteShell(inner);
    const auto capture = sandbox_process::runCommandCapture(command, 5000);
    return sandbox_process::processSucceeded(capture.process);
}

} // namespace

void ensurePool() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (config::JUDGE_DOCKER_POOL_SIZE <= 0) {
            throw std::runtime_error{"JUDGE_DOCKER_POOL_SIZE must be greater than 0"};
        }
        const auto root = work_root();
        std::filesystem::create_directories(root);
        acquire_pool_lock();
        const auto image = docker_image();
        const auto host_work_root = resolve_docker_host_path(root);
        for (int slot = 0; slot < config::JUDGE_DOCKER_POOL_SIZE; ++slot) {
            ensure_container(containerNameForSlot(slot), image, host_work_root);
        }
        spdlog::info(
            "Docker judge pool ready (image={}, size={}, work_root={})",
            image,
            config::JUDGE_DOCKER_POOL_SIZE,
            host_work_root.string()
        );
    });
}

std::string containerNameForSlot(int slot) {
    return std::string{config::JUDGE_DOCKER_POOL_NAME_PREFIX} + "-" + std::to_string(slot);
}

std::string containerWorkDirFor(const std::filesystem::path& work_dir) {
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

void ensureContainerCanSeeWorkDir(
    const std::string& container_name,
    const std::filesystem::path& work_dir
) {
    const auto container_dir = containerWorkDirFor(work_dir);
    if (container_path_exists(container_name, container_dir)) {
        return;
    }

    const auto root = work_root();
    const auto image = docker_image();
    const auto host_work_root = resolve_docker_host_path(root);
    spdlog::warn(
        "Docker judge container '{}' cannot see work directory '{}'; recreating container",
        container_name,
        container_dir
    );
    run_or_throw(
        "docker rm -f " + sandbox_process::quoteShell(container_name),
        "remove stale Docker judge pool container"
    );
    create_container(container_name, image, host_work_root);

    if (!container_path_exists(container_name, container_dir)) {
        throw std::runtime_error{
            "Docker judge container cannot see work directory after recreate: " + container_dir
        };
    }
}

std::string dockerExecCommand(
    const std::string& container_name,
    const std::string& container_work_dir,
    const std::string& inner
) {
    return "docker exec -i -w " + sandbox_process::quoteShell(container_work_dir) + " " +
        sandbox_process::quoteShell(container_name) + " sh -lc " + sandbox_process::quoteShell(inner);
}

} // namespace judge::docker_sandbox_pool
