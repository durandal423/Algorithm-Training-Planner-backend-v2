#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ENV_FILE="${ROOT_DIR}/.env"
RUN_ID="codex_$(date +%Y%m%d%H%M%S)_$$"
SERVER_LOG="/tmp/${RUN_ID}_try_crow.log"
STARTED_SERVER=0
SERVER_PID=""

if [ -f "${ENV_FILE}" ]; then
  set -a
  # shellcheck source=/dev/null
  source "${ENV_FILE}"
  set +a
fi

: "${APP_DATABASE_HOST:?APP_DATABASE_HOST is required}"
: "${APP_DATABASE_USER:?APP_DATABASE_USER is required}"
: "${APP_DATABASE_PASSWORD:?APP_DATABASE_PASSWORD is required}"
: "${APP_DATABASE_NAME:?APP_DATABASE_NAME is required}"

psql_exec() {
  docker exec -i \
    -e PGPASSWORD="${APP_DATABASE_PASSWORD}" \
    "${APP_DATABASE_HOST}" \
    psql -U "${APP_DATABASE_USER}" -d "${APP_DATABASE_NAME}" -v ON_ERROR_STOP=1 "$@"
}

cleanup() {
  if [ "${STARTED_SERVER}" = "1" ] && [ -n "${SERVER_PID}" ]; then
    kill "${SERVER_PID}" >/dev/null 2>&1 || true
    wait "${SERVER_PID}" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

if ! pgrep -f "${ROOT_DIR}/build/try-crow" >/dev/null 2>&1; then
  APP_BOOTSTRAP_ADMIN_USERNAME="${APP_BOOTSTRAP_ADMIN_USERNAME:-codex_admin}" \
  APP_BOOTSTRAP_ADMIN_EMAIL="${APP_BOOTSTRAP_ADMIN_EMAIL:-codex_admin@example.test}" \
  APP_BOOTSTRAP_ADMIN_PASSWORD="${APP_BOOTSTRAP_ADMIN_PASSWORD:-codex-admin-password}" \
    "${ROOT_DIR}/build/try-crow" >"${SERVER_LOG}" 2>&1 &
  SERVER_PID="$!"
  STARTED_SERVER=1

  for _ in $(seq 1 40); do
    if ! kill -0 "${SERVER_PID}" >/dev/null 2>&1; then
      echo "try-crow exited during startup; log: ${SERVER_LOG}" >&2
      tail -80 "${SERVER_LOG}" >&2 || true
      exit 1
    fi
    if grep -q "server starting" "${SERVER_LOG}"; then
      break
    fi
    sleep 1
  done
fi

SRC34_AC="$(<"${ROOT_DIR}/tests/fixtures/submissions/P12134_ac.cpp")"
SRC34_WA="$(<"${ROOT_DIR}/tests/fixtures/submissions/P12134_wa.cpp")"
SRC35_AC="$(<"${ROOT_DIR}/tests/fixtures/submissions/P12135_ac.cpp")"
SRC35_WA="$(<"${ROOT_DIR}/tests/fixtures/submissions/P12135_wa.cpp")"
SEED_SQL="$(mktemp "/tmp/${RUN_ID}_seed_XXXX.sql")"

cat >"${SEED_SQL}" <<SQL
BEGIN;

INSERT INTO users(username, email, display_name, password_hash, role)
VALUES ('codex_judge_load_user', 'codex_judge_load@example.test', 'Codex Judge Load', 'manual-load-no-login', 'user')
ON CONFLICT (username) DO UPDATE
SET display_name = EXCLUDED.display_name
RETURNING id;

INSERT INTO problems(code, title, source_platform, source_url, difficulty, estimated_minutes,
                     description, input_format, output_format, sample_input, sample_output, visibility)
VALUES
  ('P12134', 'P12134 Judge Load Fixture', 'local', 'https://www.luogu.com.cn/problem/P12134',
   1300, 25, 'Load-test fixture for P12134.', 'N M followed by N values.', 'Minimum value.',
   '4 2
1 5 2 4
', '3
', 'public'),
  ('P12135', 'P12135 Judge Load Fixture', 'local', 'https://www.luogu.com.cn/problem/P12135',
   1700, 40, 'Load-test fixture for P12135.', 'Two rows of equal length.', 'Minimum cost.',
   '.##.....#
.#.#.#...
', '5
', 'public')
ON CONFLICT (code) DO UPDATE
SET title = EXCLUDED.title,
    source_platform = EXCLUDED.source_platform,
    source_url = EXCLUDED.source_url,
    difficulty = EXCLUDED.difficulty,
    estimated_minutes = EXCLUDED.estimated_minutes,
    visibility = EXCLUDED.visibility;

INSERT INTO judge_configs(problem_id, enabled, language, time_limit_ms, memory_limit_mb,
                          output_limit_kb, compare_mode, float_epsilon)
SELECT id, true, 'cpp', 1000, 256, 64, 'ignore_whitespace', 1e-6
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
WHERE name LIKE 'codex-load-%'
  AND problem_id IN (SELECT id FROM problems WHERE code IN ('P12134', 'P12135'));

WITH cases(problem_code, name, input_data, expected_output, visibility, points, order_index) AS (
  VALUES
    ('P12134', 'codex-load-sample', \$TC\$4 2
1 5 2 4
\$TC\$, \$OUT\$3
\$OUT\$, 'sample', 10, 1),
    ('P12134', 'codex-load-duplicates', \$TC\$5 3
4 4 4 10 1
\$TC\$, \$OUT\$0
\$OUT\$, 'hidden', 10, 2),
    ('P12134', 'codex-load-single', \$TC\$3 1
100000 1 2
\$TC\$, \$OUT\$0
\$OUT\$, 'hidden', 10, 3),
    ('P12134', 'codex-load-window', \$TC\$6 4
1 3 8 10 11 12
\$TC\$, \$OUT\$80
\$OUT\$, 'hidden', 10, 4),
    ('P12135', 'codex-load-sample', \$TC\$.##.....#
.#.#.#...
\$TC\$, \$OUT\$5
\$OUT\$, 'sample', 10, 1),
    ('P12135', 'codex-load-same-row', \$TC\$#..#
....
\$TC\$, \$OUT\$2
\$OUT\$, 'hidden', 10, 2),
    ('P12135', 'codex-load-cross-row', \$TC\$#...
...#
\$TC\$, \$OUT\$3
\$OUT\$, 'hidden', 10, 3),
    ('P12135', 'codex-load-zigzag', \$TC\$#.#.
.#.#
\$TC\$, \$OUT\$2
\$OUT\$, 'hidden', 10, 4),
    ('P12135', 'codex-load-zero-gap', \$TC\$####
....
\$TC\$, \$OUT\$0
\$OUT\$, 'hidden', 10, 5)
)
INSERT INTO judge_test_cases(problem_id, name, input_data, expected_output, visibility, points, order_index)
SELECT p.id, c.name, c.input_data, c.expected_output, c.visibility, c.points, c.order_index
FROM cases c
JOIN problems p ON p.code = c.problem_code;

WITH payloads(problem_code, source_code, expected_verdict) AS (
  VALUES
    ('P12134', \$SRC34AC\$${SRC34_AC}
\$SRC34AC\$, 'AC'),
    ('P12134', \$SRC34WA\$${SRC34_WA}
\$SRC34WA\$, 'WA'),
    ('P12135', \$SRC35AC\$${SRC35_AC}
\$SRC35AC\$, 'AC'),
    ('P12135', \$SRC35WA\$${SRC35_WA}
\$SRC35WA\$, 'WA')
),
expanded AS (
  SELECT p.id AS problem_id,
         payloads.source_code || E'\n// codex-load-run:${RUN_ID} expected:' || payloads.expected_verdict AS source_code
  FROM payloads
  JOIN problems p ON p.code = payloads.problem_code
  CROSS JOIN generate_series(1, 25)
)
INSERT INTO submissions(problem_id, user_id, language, source_code)
SELECT problem_id,
       (SELECT id FROM users WHERE username = 'codex_judge_load_user'),
       'cpp',
       source_code
FROM expanded;

COMMIT;
SQL

psql_exec <"${SEED_SQL}" >/dev/null
rm -f "${SEED_SQL}"

echo "Seeded 100 submissions with run id ${RUN_ID}."
deadline=$((SECONDS + 240))
while [ "${SECONDS}" -lt "${deadline}" ]; do
  done_count="$(psql_exec -Atc "SELECT COUNT(*) FROM submissions WHERE source_code LIKE '%codex-load-run:${RUN_ID}%' AND status IN ('completed', 'failed', 'canceled');")"
  if [ "${done_count}" = "100" ]; then
    break
  fi
  sleep 2
done

psql_exec -c "SELECT status, COALESCE(verdict, '-') AS verdict, COUNT(*) FROM submissions WHERE source_code LIKE '%codex-load-run:${RUN_ID}%' GROUP BY status, verdict ORDER BY status, verdict;"

ac_count="$(psql_exec -Atc "SELECT COUNT(*) FROM submissions WHERE source_code LIKE '%codex-load-run:${RUN_ID}%' AND status = 'completed' AND verdict = 'AC';")"
wa_count="$(psql_exec -Atc "SELECT COUNT(*) FROM submissions WHERE source_code LIKE '%codex-load-run:${RUN_ID}%' AND status = 'completed' AND verdict = 'WA';")"
terminal_count="$(psql_exec -Atc "SELECT COUNT(*) FROM submissions WHERE source_code LIKE '%codex-load-run:${RUN_ID}%' AND status IN ('completed', 'failed', 'canceled');")"

if [ "${terminal_count}" != "100" ] || [ "${ac_count}" != "50" ] || [ "${wa_count}" != "50" ]; then
  echo "Judge load validation failed for ${RUN_ID}: terminal=${terminal_count}, AC=${ac_count}, WA=${wa_count}" >&2
  if [ -f "${SERVER_LOG}" ]; then
    echo "Server log: ${SERVER_LOG}" >&2
  fi
  exit 1
fi

echo "Judge load validation passed for ${RUN_ID}: 50 AC and 50 WA."
if [ -f "${SERVER_LOG}" ]; then
  echo "Server log: ${SERVER_LOG}"
fi
