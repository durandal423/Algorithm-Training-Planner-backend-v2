# Algorithm Training Planner Backend

面向算法训练平台的 C++ 后端服务。项目使用 Crow 提供 HTTP API，使用
PostgreSQL 存储用户、题库、提交、训练计划与后台任务，使用 Docker 沙箱执行
C++ 判题，并提供独立 worker 处理异步任务和判题队列。

## 功能概览

- 用户注册、登录、会话校验和管理员权限控制。
- 题库、标签、判题配置、测试点和提交记录管理。
- C++ 提交判题，支持 AC、WA、TLE、MLE、RE、CE、OLE、JE 等结果。
- 后台任务队列，用于提交创建和训练计划生成。
- 训练目标与训练计划生成，核心算法包含滑动窗口候选筛选和动态规划选题。
- 管理员审计日志和判题重判能力。

## 技术栈

- C++23
- CMake 3.28+
- Crow
- libpqxx / PostgreSQL
- libsodium
- nlohmann_json
- spdlog
- Catch2
- Docker CLI
- vcpkg

依赖清单见 [vcpkg.json](vcpkg.json)。

## 目录结构

```text
src/apps/        进程入口：API server、task worker、judge worker
src/api/         Crow app 与路由注册
src/domain/      auth、problem、training、judge、task、admin 等领域逻辑
src/db/          数据库连接、建库和迁移执行
src/algorithms/  训练计划生成算法
sql/             数据库迁移和种子数据
docs/            API、状态说明和判题镜像文档
tests/           Catch2 单元测试、DB 测试、手工压测脚本
docker/judge/    判题沙箱镜像
scripts/         本地辅助脚本
```

## 环境配置

CMake 配置阶段会读取仓库根目录的 `.env`。请先准备以下变量：

```env
APP_DATABASE_HOST=localhost
APP_DATABASE_PORT=5432
APP_DATABASE_USER=postgres
APP_DATABASE_PASSWORD=postgres
APP_DATABASE_NAME=algorithm_training_planner_v2
ADMIN_DATABASE_NAME=postgres
```

首次启动 API server 或 worker 时，程序会自动创建应用数据库、执行
`sql/` 下登记的迁移，并校验必需表是否存在。

可选的首个管理员 bootstrap 环境变量：

```env
APP_BOOTSTRAP_ADMIN_USERNAME=admin
APP_BOOTSTRAP_ADMIN_EMAIL=admin@example.com
APP_BOOTSTRAP_ADMIN_PASSWORD=change-me
```

只有当数据库中不存在任何 `role = 'admin'` 用户，并且三项变量齐全时，启动流程
才会创建首个管理员。

## 构建

在 devcontainer 中已经准备好 CMake、Ninja、vcpkg 和 Docker CLI。手动构建可执行：

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build -j 4
```

主要目标：

- `Algorithm-Training-Planner-backend`：HTTP API server，默认监听 `8080`。
- `Algorithm-task-worker`：后台任务 worker，消费训练计划生成和提交创建任务。
- `Algorithm-judge-worker`：判题 worker，消费 submissions 判题队列。
- `project-tests`：Catch2 测试目标。

## 运行

启动 API server：

```sh
./build/Algorithm-Training-Planner-backend
```

启动后台任务 worker：

```sh
./build/Algorithm-task-worker
```

启动判题 worker：

```sh
./build/Algorithm-judge-worker
```

默认服务地址为 `http://localhost:8080`。接口详情见 [docs/api.md](docs/api.md)。

## 判题 Docker 镜像

判题池默认使用项目自定义镜像 `atp-judge-gcc13-time:latest`，基于 `gcc:13` 并安装
GNU `/usr/bin/time`，用于采集峰值内存。

构建默认镜像：

```sh
./scripts/build_judge_image.sh
```

使用自定义镜像：

```sh
./scripts/build_judge_image.sh my-judge-image:latest
export ATP_JUDGE_DOCKER_IMAGE=my-judge-image:latest
```

更多说明见 [docs/judge-docker-image.md](docs/judge-docker-image.md)。

## 测试

运行全部已发现的 Catch2 测试：

```sh
ctest --test-dir build --output-on-failure
```

DB 测试默认跳过，需要显式开启：

```sh
RUN_DB_TESTS=1 ctest --test-dir build --output-on-failure
```

真实判题压测脚本：

```sh
tests/manual/run_judge_load_100.sh
```

该脚本会写入或更新 P12134 / P12135 的压测测试点，并提交一批 C++ 代码用于验证
判题链路。

## API 文档

- [docs/api.md](docs/api.md)：前端对接 API 文档。
- [docs/frontend-alignment.md](docs/frontend-alignment.md)：前端迁移和轮询行为说明。
- [docs/STATUS.md](docs/STATUS.md)：当前实现状态、已验证内容和已知限制。
- [sql/README.md](sql/README.md)：迁移和种子数据布局。
