FROM gcc:13

RUN apt-get update \
    && apt-get install -y --no-install-recommends time \
    && rm -rf /var/lib/apt/lists/*

RUN command -v /usr/bin/time \
    && /usr/bin/time --version >/dev/null

WORKDIR /workspace
