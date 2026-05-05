CREATE TABLE IF NOT EXISTS background_tasks (
    id BIGSERIAL PRIMARY KEY,
    type VARCHAR(96) NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'pending',
    owner_user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
    payload JSONB NOT NULL DEFAULT '{}'::jsonb,
    result JSONB,
    error_message TEXT,
    locked_by TEXT,
    locked_at TIMESTAMPTZ,
    attempt_count INTEGER NOT NULL DEFAULT 0,
    max_attempts INTEGER NOT NULL DEFAULT 3,
    run_after TIMESTAMPTZ NOT NULL DEFAULT now(),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    started_at TIMESTAMPTZ,
    finished_at TIMESTAMPTZ,
    CONSTRAINT chk_background_tasks_status CHECK (
        status IN ('pending', 'running', 'succeeded', 'failed')
    ),
    CONSTRAINT chk_background_tasks_attempt_count CHECK (attempt_count >= 0),
    CONSTRAINT chk_background_tasks_max_attempts CHECK (max_attempts > 0)
);

CREATE INDEX IF NOT EXISTS idx_background_tasks_owner_created
ON background_tasks(owner_user_id, created_at DESC);

CREATE INDEX IF NOT EXISTS idx_background_tasks_status_created
ON background_tasks(status, created_at);

CREATE INDEX IF NOT EXISTS idx_background_tasks_pending_claim
ON background_tasks(type, run_after, created_at, id)
WHERE status = 'pending';

CREATE INDEX IF NOT EXISTS idx_background_tasks_running_locked
ON background_tasks(locked_at)
WHERE status = 'running' AND locked_at IS NOT NULL;
