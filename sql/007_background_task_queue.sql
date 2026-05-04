ALTER TABLE background_tasks
    ADD COLUMN IF NOT EXISTS locked_by TEXT,
    ADD COLUMN IF NOT EXISTS locked_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS attempt_count INTEGER NOT NULL DEFAULT 0,
    ADD COLUMN IF NOT EXISTS max_attempts INTEGER NOT NULL DEFAULT 3,
    ADD COLUMN IF NOT EXISTS run_after TIMESTAMPTZ NOT NULL DEFAULT now();

ALTER TABLE background_tasks
    ADD CONSTRAINT chk_background_tasks_attempt_count
    CHECK (attempt_count >= 0)
    NOT VALID;

ALTER TABLE background_tasks
    ADD CONSTRAINT chk_background_tasks_max_attempts
    CHECK (max_attempts > 0)
    NOT VALID;

CREATE INDEX IF NOT EXISTS idx_background_tasks_pending_claim
ON background_tasks(type, run_after, created_at, id)
WHERE status = 'pending';

CREATE INDEX IF NOT EXISTS idx_background_tasks_running_locked
ON background_tasks(locked_at)
WHERE status = 'running' AND locked_at IS NOT NULL;
