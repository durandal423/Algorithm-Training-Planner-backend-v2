#!/usr/bin/env bash
set -euo pipefail

USER_NAME="${REMOTE_USER:-vscode}"
USER_HOME="$(getent passwd "${USER_NAME}" | cut -d: -f6 || true)"
if [ -z "${USER_HOME}" ]; then
  USER_HOME="/home/${USER_NAME}"
fi
USER_GROUP="$(id -gn "${USER_NAME}")"

BASHRC_PATH="${USER_HOME}/.bashrc"
touch "${BASHRC_PATH}"
if ! grep -qxF "alias cls=clear" "${BASHRC_PATH}"; then
  printf '\n%s\n' "alias cls=clear" >> "${BASHRC_PATH}"
fi
VCPKG_DOWNLOADS_DIR="${VCPKG_DOWNLOADS:-/usr/local/vcpkg-downloads}"
VCPKG_BINARY_CACHE_DIR="${VCPKG_DEFAULT_BINARY_CACHE:-/var/cache/vcpkg}"
SCCACHE_DIR_PATH="${SCCACHE_DIR:-/var/cache/sccache}"

sudo mkdir -p \
  "${USER_HOME}/.vscode-server" \
  "${VCPKG_DOWNLOADS_DIR}" \
  "${VCPKG_BINARY_CACHE_DIR}" \
  "${SCCACHE_DIR_PATH}"

sudo chown -R "${USER_NAME}:${USER_GROUP}" \
  "${USER_HOME}/.vscode-server" \
  "${VCPKG_DOWNLOADS_DIR}" \
  "${VCPKG_BINARY_CACHE_DIR}" \
  "${SCCACHE_DIR_PATH}"

git config --global --add safe.directory /usr/local/vcpkg || true
echo "[ok] git trusted /usr/local/vcpkg"

if [ -S /var/run/docker.sock ]; then
  DOCKER_GID="$(stat -c '%g' /var/run/docker.sock)"
  DOCKER_GROUP_NAME="$(getent group "${DOCKER_GID}" | cut -d: -f1 || true)"

  echo "[ok] docker socket detected"
  echo "  docker.sock gid=${DOCKER_GID}"
  echo "  docker.sock group=${DOCKER_GROUP_NAME}"

  if id -G | tr ' ' '\n' | grep -qx "${DOCKER_GID}"; then
    echo "[ok] current user has docker.sock group"
  else
    echo "[warn] current user does NOT have docker.sock group"
    echo "       rebuild with devcontainer.json runArgs: --group-add=${DOCKER_GID}"
  fi
else
  echo "[warn] /var/run/docker.sock not found"
fi

echo "[ok] cache directories ready"
echo "  VCPKG_DOWNLOADS=${VCPKG_DOWNLOADS_DIR}"
echo "  VCPKG_DEFAULT_BINARY_CACHE=${VCPKG_BINARY_CACHE_DIR}"
echo "  SCCACHE_DIR=${SCCACHE_DIR_PATH}"

echo "[ok] tool check"
command -v gcc >/dev/null 2>&1 && gcc --version | head -n 1 || true
command -v g++ >/dev/null 2>&1 && g++ --version | head -n 1 || true
command -v vcpkg >/dev/null 2>&1 && vcpkg --version | head -n 1 || true
command -v cmake >/dev/null 2>&1 && cmake --version | head -n 1 || true
command -v ninja >/dev/null 2>&1 && ninja --version || true
command -v clangd >/dev/null 2>&1 && clangd --version | head -n 1 || true
command -v sccache >/dev/null 2>&1 && sccache --version || true
command -v docker >/dev/null 2>&1 && docker --version || true
docker ps >/dev/null 2>&1 && echo "[ok] docker ps works" || echo "[warn] docker ps failed"

echo "[done] cpp devcontainer post-create finished."
