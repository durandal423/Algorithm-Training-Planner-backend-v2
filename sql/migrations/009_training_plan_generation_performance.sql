CREATE INDEX IF NOT EXISTS idx_problems_training_public_difficulty
ON problems(difficulty, estimated_minutes, id)
WHERE visibility = 'public';

CREATE INDEX IF NOT EXISTS idx_submissions_user_problem_wrong
ON submissions(user_id, problem_id)
WHERE verdict IS NOT NULL AND verdict <> 'AC';
