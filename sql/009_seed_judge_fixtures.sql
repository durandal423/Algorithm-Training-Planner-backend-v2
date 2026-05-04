-- Enable judging for the two bundled sample problems. The original seed creates
-- these problems before the judge tables exist, so keep judge data in its own
-- later migration.
INSERT INTO judge_configs(
    problem_id,
    enabled,
    language,
    time_limit_ms,
    memory_limit_mb,
    output_limit_kb,
    compare_mode,
    float_epsilon
)
SELECT
    id,
    true,
    'cpp',
    1000,
    256,
    64,
    'ignore_whitespace',
    1e-6
FROM problems
WHERE code IN ('P12134', 'P12135')
ON CONFLICT (problem_id) DO UPDATE
SET enabled = EXCLUDED.enabled,
    language = EXCLUDED.language,
    time_limit_ms = EXCLUDED.time_limit_ms,
    memory_limit_mb = EXCLUDED.memory_limit_mb,
    output_limit_kb = EXCLUDED.output_limit_kb,
    compare_mode = EXCLUDED.compare_mode,
    float_epsilon = EXCLUDED.float_epsilon;

DELETE FROM judge_test_cases
WHERE name LIKE 'seed-%'
  AND problem_id IN (
      SELECT id
      FROM problems
      WHERE code IN ('P12134', 'P12135')
  );

WITH cases(problem_code, name, input_data, expected_output, visibility, points, order_index) AS (
    VALUES
        ('P12134', 'seed-p12134-sample', $INPUT$4 2
1 5 2 4
$INPUT$, $OUTPUT$3
$OUTPUT$, 'sample', 10, 1),
        ('P12134', 'seed-p12134-duplicates', $INPUT$5 3
4 4 4 10 1
$INPUT$, $OUTPUT$0
$OUTPUT$, 'hidden', 10, 2),
        ('P12134', 'seed-p12134-single', $INPUT$3 1
100000 1 2
$INPUT$, $OUTPUT$0
$OUTPUT$, 'hidden', 10, 3),
        ('P12134', 'seed-p12134-window', $INPUT$6 4
1 3 8 10 11 12
$INPUT$, $OUTPUT$80
$OUTPUT$, 'hidden', 10, 4),
        ('P12135', 'seed-p12135-sample', $INPUT$.##.....#
.#.#.#...
$INPUT$, $OUTPUT$5
$OUTPUT$, 'sample', 10, 1),
        ('P12135', 'seed-p12135-same-row', $INPUT$#..#
....
$INPUT$, $OUTPUT$2
$OUTPUT$, 'hidden', 10, 2),
        ('P12135', 'seed-p12135-cross-row', $INPUT$#...
...#
$INPUT$, $OUTPUT$3
$OUTPUT$, 'hidden', 10, 3),
        ('P12135', 'seed-p12135-zigzag', $INPUT$#.#.
.#.#
$INPUT$, $OUTPUT$2
$OUTPUT$, 'hidden', 10, 4),
        ('P12135', 'seed-p12135-zero-gap', $INPUT$####
....
$INPUT$, $OUTPUT$0
$OUTPUT$, 'hidden', 10, 5)
)
INSERT INTO judge_test_cases(
    problem_id,
    name,
    input_data,
    expected_output,
    visibility,
    points,
    order_index
)
SELECT
    p.id,
    c.name,
    c.input_data,
    c.expected_output,
    c.visibility,
    c.points,
    c.order_index
FROM cases c
JOIN problems p ON p.code = c.problem_code;
