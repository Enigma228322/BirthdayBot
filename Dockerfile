# Multi-stage build for Birthday Bot

# ========== Builder stage ==========
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    libssl-dev \
    libcurl4-openssl-dev \
    libboost-system-dev \
    zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

# Fetch third-party libraries (submodules) if not present
RUN set -e; \
    mkdir -p /app/lib; \
    if [ ! -f /app/lib/tgbot-cpp/CMakeLists.txt ]; then \
      git clone --depth=1 https://github.com/reo7sp/tgbot-cpp.git /app/lib/tgbot-cpp; \
    fi; \
    if [ ! -f /app/lib/spdlog/CMakeLists.txt ]; then \
      git clone --depth=1 https://github.com/gabime/spdlog.git /app/lib/spdlog; \
    fi; \
    if [ ! -f /app/lib/json/CMakeLists.txt ]; then \
      git clone --depth=1 https://github.com/nlohmann/json.git /app/lib/json; \
    fi; \
    if [ ! -f /app/lib/fmt/CMakeLists.txt ]; then \
      git clone --depth=1 https://github.com/fmtlib/fmt.git /app/lib/fmt; \
    fi

RUN mkdir -p build \
 && cd build \
 && cmake -DCMAKE_BUILD_TYPE=Release .. \
 && make -j$(nproc)

# ========== Runtime stage ==========
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    ca-certificates \
    libssl3 \
    libcurl4 \
    libboost-system1.83.0 \
 && rm -rf /var/lib/apt/lists/*

# App layout
WORKDIR /data
RUN mkdir -p /app /data/logs
COPY --from=builder /app/birthday_bot /app/birthday_bot
COPY docker/entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh /app/birthday_bot

# Persistent data (birthdays.json, logs)
VOLUME ["/data"]

# Environment
ENV BOT_TOKEN=""

ENTRYPOINT ["/app/entrypoint.sh"]

