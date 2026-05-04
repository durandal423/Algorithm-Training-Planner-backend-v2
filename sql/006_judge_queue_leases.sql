CREATE INDEX IF NOT EXISTS idx_submissions_judging_locked_at ON submissions(locked_at, id)
WHERE status = 'judging';
