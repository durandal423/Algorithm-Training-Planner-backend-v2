FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-24.04

ARG HTTP_PROXY
ARG HTTPS_PROXY
ARG NO_PROXY
ARG USERNAME=vscode
ARG CMAKE_VERSION
ARG VCPKG_COMMIT
ARG DOCKER_GID=995
ARG DOCKER_GROUP=docker-host

ENV HTTP_PROXY=${HTTP_PROXY}
ENV HTTPS_PROXY=${HTTPS_PROXY}
ENV NO_PROXY=${NO_PROXY}
ENV http_proxy=${HTTP_PROXY}
ENV https_proxy=${HTTPS_PROXY}
ENV no_proxy=${NO_PROXY}
ENV VCPKG_FORCE_SYSTEM_BINARIES=1

USER root

RUN if [ -n "${http_proxy}" ]; then \
      printf '%s\n' \
        "Acquire::http::Proxy \"${http_proxy}\";" \
        "Acquire::https::Proxy \"${https_proxy}\";" \
        > /etc/apt/apt.conf.d/99proxy; \
    fi

RUN mkdir -p /home/${USERNAME}/.vscode-server \
    && chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}/.vscode-server

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        git \
        tar \
        unzip \
        wget \
        zip \
        ninja-build \
        clangd \
        pkg-config \
        sccache \
        bison \
        flex \
        autoconf \
        autoconf-archive \
        automake \
        libtool \
        m4 \
        gnupg \
    && rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    . /etc/os-release; \
    case "${ID}" in \
      debian) docker_repo="https://download.docker.com/linux/debian"; docker_codename="${VERSION_CODENAME}" ;; \
      ubuntu) docker_repo="https://download.docker.com/linux/ubuntu"; docker_codename="${UBUNTU_CODENAME:-${VERSION_CODENAME}}" ;; \
      *) echo "Unsupported distribution for Docker CLI: ${ID}" >&2; exit 1 ;; \
    esac; \
    install -m 0755 -d /etc/apt/keyrings; \
    curl -fsSL "${docker_repo}/gpg" -o /etc/apt/keyrings/docker.asc; \
    chmod a+r /etc/apt/keyrings/docker.asc; \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] ${docker_repo} ${docker_codename} stable" \
      > /etc/apt/sources.list.d/docker.list; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
      docker-ce-cli \
      docker-buildx-plugin \
      docker-compose-plugin; \
    rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    if ! getent group "${DOCKER_GID}" >/dev/null; then \
      groupadd --gid "${DOCKER_GID}" "${DOCKER_GROUP}"; \
    fi; \
    DOCKER_GROUP_NAME="$(getent group "${DOCKER_GID}" | cut -d: -f1)"; \
    usermod -aG "${DOCKER_GROUP_NAME}" "${USERNAME}"

USER ${USERNAME}

RUN git config --global --add safe.directory /usr/local/vcpkg \
    && if [ "$(git -C /usr/local/vcpkg rev-parse --is-shallow-repository)" = "true" ]; then \
         git -C /usr/local/vcpkg fetch --unshallow --tags origin; \
       else \
         git -C /usr/local/vcpkg fetch --tags --prune origin; \
       fi \
    && git -C /usr/local/vcpkg checkout --force "${VCPKG_COMMIT}" \
    && /usr/local/vcpkg/bootstrap-vcpkg.sh -disableMetrics

USER root

RUN arch="$(dpkg --print-architecture)" && \
    case "$arch" in \
        amd64) cmake_arch="linux-x86_64" ;; \
        arm64) cmake_arch="linux-aarch64" ;; \
        *) echo "Unsupported architecture: $arch" >&2; exit 1 ;; \
    esac && \
    wget -q "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-${cmake_arch}.sh" \
        -O /tmp/cmake-installer.sh && \
    sh /tmp/cmake-installer.sh --skip-license --prefix=/usr/local && \
    rm -f /tmp/cmake-installer.sh

RUN gcc --version \
    && g++ --version \
    && /usr/local/vcpkg/vcpkg --version \
    && cmake --version \
    && docker --version
