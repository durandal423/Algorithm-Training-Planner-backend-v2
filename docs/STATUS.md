# 当前状态

本项目已经从最小 Crow 骨架推进到“可用的第一版 OJ 后端业务闭环”。当前后端基于 Crow + libpqxx + libsodium + Docker CLI，领域代码按 `src/domain/*` 拆分，已经具备用户认证、题库管理、C++ 提交判题、管理员判题配置、训练目标与训练计划生成、核心测试与真实判题压测。

## 已完成

- 数据库迁移：
  - `sql/003_oj_schema.sql` 创建 OJ 核心表。
  - `src/db/schema.cpp` 维护 version `1, 2, 3`，并跳过空白 SQL 文件。
  - 全新库启动时会自动检查数据库、执行迁移、验证必需表存在。
- 认证与用户：
  - 注册、登录、当前用户、登出 API 已实现。
  - 密码使用 `libsodium` Argon2id hash。
  - 登录 token 只保存 hash 到 `user_sessions`。
  - 支持 `Authorization: Bearer <token>`。
  - 支持 `requireUser` / `requireAdmin`。
- 管理员 bootstrap：
  - 启动时如果库中没有任何 `role = 'admin'` 用户，会读取：
    - `APP_BOOTSTRAP_ADMIN_USERNAME`
    - `APP_BOOTSTRAP_ADMIN_EMAIL`
    - `APP_BOOTSTRAP_ADMIN_PASSWORD`
  - 三项齐全才创建首个管理员。
  - 已有 admin 时不会重复创建，也不会覆盖密码。
- 题库 API：
  - 公开查询题目和标签。
  - 管理员创建、更新、删除题目。
- 判题系统：
  - 用户提交 C++ 代码进入 `submissions` 队列。
  - 进程内 `JudgeDispatcher` 启动固定线程池。
  - Judge 线程使用 `FOR UPDATE SKIP LOCKED` 抢 `queued` submission，避免重复领取。
  - Docker 判题池使用 `atp-judge-sandbox-0..N` 命名。
  - 默认 Docker 镜像为 `gcc:13`。
  - 支持 `exact`、`trim_trailing`、`ignore_whitespace`、`float_epsilon` 输出比较。
- Judge API：
  - 普通用户可提交代码、查看自己的提交、列出自己的提交。
  - 管理员可维护判题配置和测试点。
  - 管理员可列出所有提交、按题目或用户筛选。
  - 管理员可重判提交。
- 训练计划 API：
  - 用户可维护训练目标 CRUD。
  - 用户可根据训练目标生成训练计划。
  - 生成策略按标签、难度范围、可见性过滤，再按标签命中、难度、错题倾向、预计耗时做简单贪心选择。
  - 用户可查看训练计划列表、详情、计划项列表。
  - 判题完成后，会回写该用户最近相关 `draft` / `active` 训练计划项的 `last_submission_id`、`last_verdict`、`status`。
- 测试与验证：
  - 新增 Catch2 测试目标 `try-crow-tests`。
  - 测试按方向拆分到 `tests/unit`、`tests/db`、`tests/fixtures`、`tests/manual`。
  - DB 测试通过 `RUN_DB_TESTS=1` 开启。
  - 新增 P12134 / P12135 真实判题压测脚本。

## 当前 API 文档

前端对接请看 `docs/api.md`。该文档按领域整理了：

- 认证方式。
- 请求体字段。
- 响应字段。
- 权限要求。
- 查询参数。
- 常见错误结构。
- 提交轮询和管理员配置建议。

## 当前 API 概览

- `POST /api/auth/register`
- `POST /api/auth/login`
- `GET /api/auth/me`
- `POST /api/auth/logout`
- `GET /api/problems`
- `GET /api/problems/<id>`
- `GET /api/tags`
- `POST /api/admin/problems`
- `PUT /api/admin/problems/<id>`
- `DELETE /api/admin/problems/<id>`
- `GET /api/submissions`
- `POST /api/problems/<id>/submissions`
- `GET /api/submissions/<id>`
- `GET /api/admin/submissions`
- `POST /api/admin/submissions/<id>/rejudge`
- `GET /api/admin/problems/<id>/judge-config`
- `PUT /api/admin/problems/<id>/judge-config`
- `GET /api/admin/problems/<id>/judge-test-cases`
- `POST /api/admin/problems/<id>/judge-test-cases`
- `PUT /api/admin/judge-test-cases/<id>`
- `DELETE /api/admin/judge-test-cases/<id>`
- `GET /api/training/goals`
- `POST /api/training/goals`
- `GET /api/training/goals/<id>`
- `PUT /api/training/goals/<id>`
- `DELETE /api/training/goals/<id>`
- `POST /api/training/plans/generate`
- `GET /api/training/plans`
- `GET /api/training/plans/<id>`
- `GET /api/training/plans/<id>/items`
- `GET /api/admin/audit-logs`

## 已验证

- `cmake --build build -j2` 通过。
- `./build/try-crow-tests` 通过。
- `RUN_DB_TESTS=1 ./build/try-crow-tests` 通过。
- `ctest --test-dir build --output-on-failure` 通过。
- `timeout 12s ./build/try-crow` 可启动到数据库迁移检查、管理员 bootstrap 检查、Docker judge pool 初始化、JudgeDispatcher 启动、Crow server 启动；`exit_code=124` 是 `timeout` 的预期结果。
- `tests/manual/run_judge_load_100.sh` 已完成真实判题压测：
  - 向 P12134 / P12135 写入 `codex-load-*` 测试点。
  - 提交 100 条 C++ 代码。
  - 最终结果为 `50 AC / 50 WA`。
- 压测过程中发现并修复：
  - `findUserByActiveSession` join 后 `id` 列歧义问题。
  - 判题完成回写训练计划项时 PostgreSQL 参数类型推断冲突问题。

## 当前数据注意事项

- 本地数据库 `algorithm_training_planner_v2` 已记录 `schema_migrations` version `1, 2, 3`。
- 压测脚本会在题库中保留或更新 P12134 / P12135 的 `codex-load-*` 测试点。
- 最近一次真实压测提交保留在数据库中，便于复查判题记录。
- `.gitignore` 有用户已有修改，不应随意回退。

## 已知限制

- 错误响应目前统一形态为 `{ "error": string }`，但 helper 仍分散在各 routes 文件中。
- 列表接口目前没有返回 total count，只返回 `items`。
- `GET /api/problems` 当前只接入了 `q`、`tag`、`limit`、`offset` 查询参数，难度范围字段在 repository 中存在但路由暂未暴露。
- 训练计划生成策略是第一版直观贪心策略，不是复杂推荐算法。
- 训练记录 `training_records` 仍只有 repository 创建方法，暂未提供完整前端 CRUD API。
- 只支持 C++ 判题。
- 不使用 JWT；继续使用服务端 session token hash。
