FROM mcr.microsoft.com/devcontainers/cpp:1-bookworm@sha256:7276064ebe6540274de9c8e29097a955724b21b1c7f2b087587b55a51f7a958b

ARG HTTP_PROXY
ARG HTTPS_PROXY
ARG NO_PROXY

ENV HTTP_PROXY=${HTTP_PROXY}
ENV HTTPS_PROXY=${HTTPS_PROXY}
ENV NO_PROXY=${NO_PROXY}
ENV http_proxy=${HTTP_PROXY}
ENV https_proxy=${HTTPS_PROXY}
ENV no_proxy=${NO_PROXY}
ARG GCC_VERSION
ARG CMAKE_VERSION
ARG VCPKG_COMMIT

ENV VCPKG_FORCE_SYSTEM_BINARIES=1

RUN mkdir -p /home/vscode/.vscode-server \
    && chown -R vscode:vscode /home/vscode/.vscode-server

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
        automake \
        libtool \
        m4 \
        gcc-${GCC_VERSION} \
        g++-${GCC_VERSION} \
        docker.io \
    && rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VERSION} 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_VERSION} 100

USER vscode

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
    && cmake --version
