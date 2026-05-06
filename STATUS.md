# Task Queue Refactor Status

## Completed

- Added PostgreSQL queue metadata for `background_tasks` in migration `007_background_task_queue.sql`.
- Added durable task submission and queue-claiming code:
  - `TaskSubmissionService`
  - `TaskQueueRepository`
  - `TaskHandlerRegistry`
  - `TaskExecutionContext`
  - `TaskWorkerRuntime`
- Added a new `Algorithm-task-worker` executable.
- Moved process entry points into `src/apps/`:
  - `api_server_main.cpp`
  - `task_worker_main.cpp`
  - `judge_worker_main.cpp`
- Removed the old in-process `TaskScheduler` implementation.
- Migrated these background task types to durable worker execution:
  - `training.plan.generate`
  - `training.plan.generate.bruteforce`
  - `judge.submission.create`
- Kept `judge-worker` responsible for the existing `submissions` queue.
- API server now submits durable tasks instead of enqueueing in-process lambdas.
- `auth.register` and `auth.login` now use ordinary synchronous responses and no longer
  write to `background_tasks`.

## Important Design Notes

- `auth.register` and `auth.login` are intentionally not durable tasks. This avoids storing
  plaintext passwords in `background_tasks.payload`.
- If auth must become durable-worker based later, add payload encryption and result cleanup/TTL first.
- Training plan generation dedupe/coalescing is not implemented yet.

## Verification

- `cmake --build build -j 4` passes.
- `ctest --test-dir build --output-on-failure` passes.
- `RUN_DB_TESTS=1 ctest --test-dir build -R "background task repository" --output-on-failure` passes.
- `RUN_DB_TESTS=1 ctest --test-dir build -R "task queue repository" --output-on-failure` passes.
- Migration 7 was applied successfully to the Docker PostgreSQL database.

## Known Environment Issue

- Running the full `RUN_DB_TESTS=1 ctest` against the shared development database can fail
  because old `submissions` result outside the test fixtures are still in `queued`/`judging`
  state. The judge queue tests consume the global queue and assume an isolated database.
  This is pre-existing test isolation debt, not caused by the background task refactor.

## TODO

- Add focused DB tests for `TaskQueueRepository::recoverStaleRunningTasks()`.
- Add training task dedupe/coalescing by `user_id + goal_id + algorithm` if product behavior needs it.
- Consider splitting CMake source lists into shared object/static library targets to reduce duplication
  between API, task-worker, judge-worker, and tests.
