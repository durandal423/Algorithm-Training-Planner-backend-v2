-- Backfill tag links for the synthetic training problems generated as T0033..T1000.
-- This is intentionally idempotent so existing development databases can safely
-- receive the problem_tags rows after 002_seed.sql has already been recorded.
WITH tag_vocab AS (
    SELECT ARRAY[
        '双指针',
        '滑动窗口',
        '动态规划',
        '贪心',
        '图论',
        '搜索',
        '数论',
        '数据结构',
        '状态压缩',
        '排序',
        '哈希表',
        '连通性',
        '前缀和',
        '二分'
    ]::text[] AS tags
),
generated_problem_rows AS (
    SELECT
        code,
        substring(code FROM 2)::integer AS n
    FROM problems
    WHERE code ~ '^T[0-9]{4}$'
),
generated_problem_tag_seed AS (
    SELECT
        generated_problem_rows.code AS problem_code,
        selected.tag_name
    FROM generated_problem_rows
    CROSS JOIN tag_vocab
    CROSS JOIN LATERAL (
        VALUES
            (tag_vocab.tags[((generated_problem_rows.n - 1) % array_length(tag_vocab.tags, 1)) + 1]),
            (tag_vocab.tags[((generated_problem_rows.n + 3) % array_length(tag_vocab.tags, 1)) + 1]),
            (tag_vocab.tags[((generated_problem_rows.n + 7) % array_length(tag_vocab.tags, 1)) + 1])
    ) AS selected(tag_name)
    WHERE generated_problem_rows.n BETWEEN 33 AND 1000
)
INSERT INTO problem_tags(problem_id, tag_id)
SELECT p.id, t.id
FROM generated_problem_tag_seed
JOIN problems p ON p.code = generated_problem_tag_seed.problem_code
JOIN tags t ON t.name = generated_problem_tag_seed.tag_name
ON CONFLICT DO NOTHING;
