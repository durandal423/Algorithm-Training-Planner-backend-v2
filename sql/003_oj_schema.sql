CREATE TABLE IF NOT EXISTS judge_configs (
    id BIGSERIAL PRIMARY KEY,
    problem_id BIGINT NOT NULL UNIQUE REFERENCES problems(id) ON DELETE CASCADE,
    enabled BOOLEAN NOT NULL DEFAULT FALSE,
    language VARCHAR(32) NOT NULL DEFAULT 'cpp',
    compile_command_template TEXT,
    run_command_template TEXT,
    time_limit_ms INTEGER NOT NULL DEFAULT 1000,
    memory_limit_mb INTEGER NOT NULL DEFAULT 256,
    output_limit_kb INTEGER NOT NULL DEFAULT 1024,
    compare_mode VARCHAR(32) NOT NULL DEFAULT 'ignore_whitespace',
    float_epsilon DOUBLE PRECISION NOT NULL DEFAULT 1e-6,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT chk_judge_configs_language CHECK (language IN ('cpp')),
    CONSTRAINT chk_judge_configs_compare_mode CHECK (
        compare_mode IN ('exact', 'trim_trailing', 'ignore_whitespace', 'float_epsilon')
    ),
    CONSTRAINT chk_judge_configs_limits CHECK (
        time_limit_ms > 0 AND memory_limit_mb > 0 AND output_limit_kb > 0
    )
);

CREATE TABLE IF NOT EXISTS judge_test_cases (
    id BIGSERIAL PRIMARY KEY,
    problem_id BIGINT NOT NULL REFERENCES problems(id) ON DELETE CASCADE,
    name VARCHAR(128),
    input_data TEXT NOT NULL,
    expected_output TEXT,
    visibility VARCHAR(32) NOT NULL DEFAULT 'hidden',
    points INTEGER NOT NULL DEFAULT 1,
    order_index INTEGER NOT NULL DEFAULT 0,
    time_limit_ms INTEGER,
    memory_limit_mb INTEGER,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT chk_judge_test_cases_visibility CHECK (visibility IN ('sample', 'hidden', 'admin_only')),
    CONSTRAINT chk_judge_test_cases_points CHECK (points >= 0),
    CONSTRAINT chk_judge_test_cases_limits CHECK (
        (time_limit_ms IS NULL OR time_limit_ms > 0) AND
        (memory_limit_mb IS NULL OR memory_limit_mb > 0)
    )
);

CREATE TABLE IF NOT EXISTS submissions (
    id BIGSERIAL PRIMARY KEY,
    problem_id BIGINT NOT NULL REFERENCES problems(id) ON DELETE CASCADE,
    user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
    language VARCHAR(32) NOT NULL DEFAULT 'cpp',
    source_code TEXT NOT NULL,
    status VARCHAR(32) NOT NULL DEFAULT 'queued',
    verdict VARCHAR(32),
    score INTEGER NOT NULL DEFAULT 0,
    compile_stdout TEXT,
    compile_stderr TEXT,
    compile_time_ms INTEGER,
    max_time_ms INTEGER,
    max_memory_kb INTEGER,
    locked_by TEXT,
    locked_at TIMESTAMPTZ,
    attempt_count INTEGER NOT NULL DEFAULT 0,
    submitted_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    started_at TIMESTAMPTZ,
    finished_at TIMESTAMPTZ,
    CONSTRAINT chk_submissions_language CHECK (language IN ('cpp')),
    CONSTRAINT chk_submissions_status CHECK (status IN ('queued', 'judging', 'completed', 'failed', 'canceled')),
    CONSTRAINT chk_submissions_verdict CHECK (
        verdict IS NULL OR verdict IN ('AC', 'WA', 'TLE', 'MLE', 'RE', 'CE', 'OLE', 'JE')
    ),
    CONSTRAINT chk_submissions_score CHECK (score >= 0),
    CONSTRAINT chk_submissions_attempt_count CHECK (attempt_count >= 0)
);

CREATE TABLE IF NOT EXISTS submission_results (
    id BIGSERIAL PRIMARY KEY,
    submission_id BIGINT NOT NULL REFERENCES submissions(id) ON DELETE CASCADE,
    test_case_id BIGINT REFERENCES judge_test_cases(id) ON DELETE SET NULL,
    order_index INTEGER NOT NULL DEFAULT 0,
    status VARCHAR(32) NOT NULL DEFAULT 'completed',
    verdict VARCHAR(32),
    time_ms INTEGER,
    memory_kb INTEGER,
    exit_code INTEGER,
    stdout_sample TEXT,
    stderr_sample TEXT,
    message TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT chk_submission_results_status CHECK (status IN ('queued', 'judging', 'completed', 'failed')),
    CONSTRAINT chk_submission_results_verdict CHECK (
        verdict IS NULL OR verdict IN ('AC', 'WA', 'TLE', 'MLE', 'RE', 'CE', 'OLE', 'JE')
    )
);

CREATE TABLE IF NOT EXISTS training_goals (
    id BIGSERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(128) NOT NULL,
    target_count INTEGER NOT NULL,
    time_budget_minutes INTEGER NOT NULL,
    difficulty_min INTEGER,
    difficulty_max INTEGER,
    prefer_wrong_problems BOOLEAN NOT NULL DEFAULT TRUE,
    prefer_weak_tags BOOLEAN NOT NULL DEFAULT TRUE,
    difficulty_weight INTEGER NOT NULL DEFAULT 10,
    tag_coverage_weight INTEGER NOT NULL DEFAULT 20,
    wrong_problem_weight INTEGER NOT NULL DEFAULT 15,
    weak_tag_weight INTEGER NOT NULL DEFAULT 15,
    estimated_time_weight INTEGER NOT NULL DEFAULT 1,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT chk_training_goals_target_count CHECK (target_count > 0),
    CONSTRAINT chk_training_goals_time_budget CHECK (time_budget_minutes > 0),
    CONSTRAINT chk_training_goals_difficulty_range CHECK (
        difficulty_min IS NULL OR difficulty_max IS NULL OR difficulty_min <= difficulty_max
    )
);

CREATE TABLE IF NOT EXISTS training_goal_tags (
    goal_id BIGINT NOT NULL REFERENCES training_goals(id) ON DELETE CASCADE,
    tag_id BIGINT NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (goal_id, tag_id)
);

CREATE TABLE IF NOT EXISTS training_plans (
    id BIGSERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    goal_id BIGINT REFERENCES training_goals(id) ON DELETE SET NULL,
    name VARCHAR(128) NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'draft',
    candidate_count INTEGER NOT NULL DEFAULT 0,
    selected_count INTEGER NOT NULL DEFAULT 0,
    total_estimated_minutes INTEGER NOT NULL DEFAULT 0,
    algorithm_summary TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT chk_training_plans_status CHECK (status IN ('draft', 'active', 'completed', 'archived')),
    CONSTRAINT chk_training_plans_counts CHECK (
        candidate_count >= 0 AND selected_count >= 0 AND total_estimated_minutes >= 0
    )
);

CREATE TABLE IF NOT EXISTS training_plan_items (
    id BIGSERIAL PRIMARY KEY,
    plan_id BIGINT NOT NULL REFERENCES training_plans(id) ON DELETE CASCADE,
    problem_id BIGINT NOT NULL REFERENCES problems(id) ON DELETE CASCADE,
    order_index INTEGER NOT NULL,
    estimated_minutes INTEGER NOT NULL,
    score INTEGER NOT NULL DEFAULT 0,
    selected_reason TEXT,
    status VARCHAR(32) NOT NULL DEFAULT 'not_started',
    last_submission_id BIGINT REFERENCES submissions(id) ON DELETE SET NULL,
    last_verdict VARCHAR(32),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT chk_training_plan_items_status CHECK (status IN ('not_started', 'failed', 'completed')),
    CONSTRAINT chk_training_plan_items_verdict CHECK (
        last_verdict IS NULL OR last_verdict IN ('AC', 'WA', 'TLE', 'MLE', 'RE', 'CE', 'OLE', 'JE')
    ),
    CONSTRAINT chk_training_plan_items_estimated CHECK (estimated_minutes > 0)
);

CREATE TABLE IF NOT EXISTS training_records (
    id BIGSERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    plan_id BIGINT REFERENCES training_plans(id) ON DELETE SET NULL,
    plan_item_id BIGINT REFERENCES training_plan_items(id) ON DELETE SET NULL,
    problem_id BIGINT NOT NULL REFERENCES problems(id) ON DELETE CASCADE,
    submission_id BIGINT REFERENCES submissions(id) ON DELETE SET NULL,
    is_finished BOOLEAN NOT NULL DEFAULT FALSE,
    is_first_try_ac BOOLEAN NOT NULL DEFAULT FALSE,
    actual_minutes INTEGER,
    error_type VARCHAR(64),
    review_note TEXT,
    practiced_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT chk_training_records_actual_minutes CHECK (
        actual_minutes IS NULL OR actual_minutes >= 0
    )
);

CREATE TABLE IF NOT EXISTS audit_logs (
    id BIGSERIAL PRIMARY KEY,
    actor_user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
    action VARCHAR(128) NOT NULL,
    target_type VARCHAR(64) NOT NULL,
    target_id BIGINT,
    metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_judge_test_cases_problem_order ON judge_test_cases(problem_id, order_index);
CREATE INDEX IF NOT EXISTS idx_submissions_problem_finished ON submissions(problem_id, finished_at DESC, id DESC);
CREATE INDEX IF NOT EXISTS idx_submissions_user_submitted ON submissions(user_id, submitted_at DESC);
CREATE INDEX IF NOT EXISTS idx_submissions_queue ON submissions(status, submitted_at, id)
WHERE status = 'queued';
CREATE INDEX IF NOT EXISTS idx_submissions_judging_locked_at ON submissions(locked_at, id)
WHERE status = 'judging';
CREATE INDEX IF NOT EXISTS idx_submission_results_submission ON submission_results(submission_id, order_index);

CREATE INDEX IF NOT EXISTS idx_training_goals_user ON training_goals(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_training_plans_user ON training_plans(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_training_plan_items_plan_order ON training_plan_items(plan_id, order_index);
CREATE INDEX IF NOT EXISTS idx_training_plan_items_problem ON training_plan_items(problem_id);
CREATE INDEX IF NOT EXISTS idx_training_records_user_practiced ON training_records(user_id, practiced_at DESC);
CREATE INDEX IF NOT EXISTS idx_training_records_problem ON training_records(problem_id);

CREATE INDEX IF NOT EXISTS idx_audit_logs_actor_created ON audit_logs(actor_user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_audit_logs_target ON audit_logs(target_type, target_id);

DROP TRIGGER IF EXISTS trg_judge_configs_updated_at ON judge_configs;
CREATE TRIGGER trg_judge_configs_updated_at
BEFORE UPDATE ON judge_configs
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

DROP TRIGGER IF EXISTS trg_judge_test_cases_updated_at ON judge_test_cases;
CREATE TRIGGER trg_judge_test_cases_updated_at
BEFORE UPDATE ON judge_test_cases
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();

DROP TRIGGER IF EXISTS trg_training_plan_items_updated_at ON training_plan_items;
CREATE TRIGGER trg_training_plan_items_updated_at
BEFORE UPDATE ON training_plan_items
FOR EACH ROW
EXECUTE FUNCTION set_updated_at();
