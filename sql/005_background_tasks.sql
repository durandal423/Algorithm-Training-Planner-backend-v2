CREATE TABLE IF NOT EXISTS background_tasks (
    id BIGSERIAL PRIMARY KEY,
    type VARCHAR(96) NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'pending',
    owner_user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
    payload JSONB NOT NULL DEFAULT '{}'::jsonb,
    result JSONB,
    error_message TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    started_at TIMESTAMPTZ,
    finished_at TIMESTAMPTZ,
    CONSTRAINT chk_background_tasks_status CHECK (
        status IN ('pending', 'running', 'succeeded', 'failed')
    )
);

CREATE INDEX IF NOT EXISTS idx_background_tasks_owner_created
ON background_tasks(owner_user_id, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_background_tasks_status_created
ON background_tasks(status, created_at);

ALTER TABLE training_goals
    ADD CONSTRAINT chk_training_goals_target_count_upper
    CHECK (target_count <= 30)
    NOT VALID;

ALTER TABLE training_goals
    ADD CONSTRAINT chk_training_goals_time_budget_upper
    CHECK (time_budget_minutes <= 480)
    NOT VALID;
