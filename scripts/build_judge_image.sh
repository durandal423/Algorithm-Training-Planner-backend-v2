#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image_name="${1:-${ATP_JUDGE_DOCKER_IMAGE:-atp-judge-gcc13-time:latest}}"

docker build \
    -f "${repo_root}/docker/judge/Dockerfile" \
    -t "${image_name}" \
    "${repo_root}"

echo "Built judge Docker image: ${image_name}"
