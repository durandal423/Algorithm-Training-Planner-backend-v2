# SQL layout

`migrations/` contains schema migrations grouped by the API/docs domains:

- `001_core_auth_problem_schema.sql`: users, sessions, problems, tags.
- `003_judge_training_admin_schema.sql`: judge tables, submissions, training
  plans, training records, audit logs.
- `005_background_tasks.sql`: async task table and durable queue metadata.

`seeds/` contains versioned data migrations that may still be registered by
`src/db/schema.cpp`:

- `002_problem_catalog.sql`: problem/tag seed data, including generated
  practice-bank tag links.
- `009_judge_sample_fixtures.sql`: bundled judge configs and test cases for the
  two sample problems.
- `010_default_admin_user.sql`: default admin account seed; register it last.

Version `004` is intentionally still unused because older local databases may
already have recorded a deleted version `4`.
