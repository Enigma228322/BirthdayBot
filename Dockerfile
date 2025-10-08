# Multi-stage build for Birthday Bot

# ========== Builder stage ==========
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libssl-dev \
    libcurl4-openssl-dev \
    libboost-system-dev \
    zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

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

