#pragma once

#include <filesystem>
#include <string>

namespace judge::docker_sandbox_pool {

void ensurePool();
std::string containerNameForSlot(int slot);
std::string containerWorkDirFor(const std::filesystem::path& work_dir);
void ensureContainerCanSeeWorkDir(
    const std::string& container_name,
    const std::filesystem::path& work_dir
);
std::string dockerExecCommand(
    const std::string& container_name,
    const std::string& container_work_dir,
    const std::string& inner
);

} // namespace judge::docker_sandbox_pool
